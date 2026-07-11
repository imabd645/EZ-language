#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"

#include <curl/curl.h>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <future>
#include <algorithm>
#include <map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

// Include EZFuture (Windows-native future) after windows headers

#include "runtime/EZFuture.h"
#include "eventloop/EventLoop.h"
#include <mutex>
#include <vector>

static std::mutex g_curl_pool_mutex;
static std::vector<CURL*> g_curl_pool;

static CURL* get_curl_handle() {
    std::lock_guard<std::mutex> lock(g_curl_pool_mutex);
    if (!g_curl_pool.empty()) {
        CURL* c = g_curl_pool.back();
        g_curl_pool.pop_back();
        curl_easy_reset(c);
        return c;
    }
    return curl_easy_init();
}

static void release_curl_handle(CURL* c) {
    if (c) {
        std::lock_guard<std::mutex> lock(g_curl_pool_mutex);
        g_curl_pool.push_back(c);
    }
}

#ifdef TokenType
#undef TokenType
#define RESTORE_TOKEN_TYPE
#endif

#include <uv.h>

#ifdef RESTORE_TOKEN_TYPE
#define TokenType TokenKind
#endif

static CURLM *g_curl_multi = nullptr;
static uv_timer_t g_timeout;

struct CurlContext {
  uv_poll_t poll_handle;
  curl_socket_t sockfd;
};

struct RequestState {
  std::string response;
  std::shared_ptr<EZFuture> ezFut;
  struct curl_slist* headers = nullptr;
};

static void check_multi_info();
static void curl_perform(uv_poll_t *req, int status, int events);
static void on_timeout(uv_timer_t *req);
static int start_timeout(CURLM *multi, long timeout_ms, void *userp);
static void close_cb(uv_handle_t* handle);
static int handle_socket(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp);

static void check_multi_info() {
  CURLMsg *message;
  int pending;
  
  while((message = curl_multi_info_read(g_curl_multi, &pending))) {
    switch(message->msg) {
    case CURLMSG_DONE: {
      CURL *easy_handle = message->easy_handle;
      RequestState *state = nullptr;
      curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &state);
      
      long response_code;
      curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
      
      if(message->data.result != CURLE_OK) {
        Value err = Value::makeDictionary();
        err.asDictionaryPtr()->modifyMap([&](auto& m) { m["error"] = Value("Fetch failed: " + std::string(curl_easy_strerror(message->data.result))); });
        state->ezFut->set(err);
      } else {
        state->ezFut->set(Value(state->response));
      }
      
      curl_multi_remove_handle(g_curl_multi, easy_handle);
      if(state->headers) curl_slist_free_all(state->headers);
      curl_easy_cleanup(easy_handle);
      delete state;
      EventLoop::instance().release();
      break;
    }
    default:
      break;
    }
  }
}

static void curl_perform(uv_poll_t *req, int status, int events) {
  int running_handles;
  int flags = 0;
  if(events & UV_READABLE) flags |= CURL_CSELECT_IN;
  if(events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;
  
  CurlContext *context = (CurlContext*)req->data;
  curl_multi_socket_action(g_curl_multi, context->sockfd, flags, &running_handles);
  check_multi_info();
}

static void on_timeout(uv_timer_t *req) {
  int running_handles;
  curl_multi_socket_action(g_curl_multi, CURL_SOCKET_TIMEOUT, 0, &running_handles);
  check_multi_info();
}

static int start_timeout(CURLM *multi, long timeout_ms, void *userp) {
  if(timeout_ms < 0) {
    uv_timer_stop(&g_timeout);
  } else {
    if(timeout_ms == 0) timeout_ms = 1;
    uv_timer_start(&g_timeout, on_timeout, timeout_ms, 0);
  }
  return 0;
}

static void close_cb(uv_handle_t* handle) {
  CurlContext* context = (CurlContext*) handle->data;
  delete context;
}

static int handle_socket(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp) {
  CurlContext *curl_context;
  int events = 0;
  
  switch(action) {
  case CURL_POLL_IN:
  case CURL_POLL_OUT:
  case CURL_POLL_INOUT:
    curl_context = socketp ? (CurlContext*)socketp : new CurlContext();
    if(!socketp) {
      curl_context->sockfd = s;
      uv_poll_init_socket(EventLoop::instance().getLoop(), &curl_context->poll_handle, s);
      curl_context->poll_handle.data = curl_context;
      curl_multi_assign(g_curl_multi, s, curl_context);
    }
    
    if(action != CURL_POLL_IN) events |= UV_WRITABLE;
    if(action != CURL_POLL_OUT) events |= UV_READABLE;
    
    uv_poll_start(&curl_context->poll_handle, events, curl_perform);
    break;
  case CURL_POLL_REMOVE:
    if(socketp) {
      uv_poll_stop(&((CurlContext*)socketp)->poll_handle);
      uv_close((uv_handle_t*)&((CurlContext*)socketp)->poll_handle, close_cb);
      curl_multi_assign(g_curl_multi, s, NULL);
    }
    break;
  default:
    abort();
  }
  return 0;
}

static size_t HttpWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void registerNetBuiltins(RuntimeContext& interp) {
    static int curl_init_checker = []() { curl_global_init(CURL_GLOBAL_DEFAULT); return 0; }();
    (void)curl_init_checker;

    if (!g_curl_multi) {
        g_curl_multi = curl_multi_init();
        uv_timer_init(EventLoop::instance().getLoop(), &g_timeout);
        curl_multi_setopt(g_curl_multi, CURLMOPT_SOCKETFUNCTION, handle_socket);
        curl_multi_setopt(g_curl_multi, CURLMOPT_TIMERFUNCTION, start_timeout);
    }

    interp.defineGlobal("url_encode", Value::makeNativeFunction("url_encode", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string s = args[0].toString();
            CURL* curl = curl_easy_init();
            char* output = curl_easy_escape(curl, s.c_str(), (int)s.length());
            std::string res(output);
            curl_free(output);
            curl_easy_cleanup(curl);
            return Value(res);
        }));

    interp.defineGlobal("url_decode", Value::makeNativeFunction("url_decode", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string s = args[0].toString();
            CURL* curl = curl_easy_init();
            int outlen;
            char* output = curl_easy_unescape(curl, s.c_str(), (int)s.length(), &outlen);
            std::string res(output, outlen);
            curl_free(output);
            curl_easy_cleanup(curl);
            return Value(res);
        }));

    interp.defineGlobal("http_get", Value::makeNativeFunction("http_get", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty()) { interp.throwException("TypeError", "http_get() expects URL", 0, ""); return Value(); }
            std::string url = args[0].toString();
            CURL* curl = get_curl_handle();
            if (!curl) { interp.throwException("NetworkError", "CURL init failed", 0, ""); return Value(); }
            std::string res;
            long ssl_verify = 1L;
            long ssl_verifyhost = 2L;
            struct curl_slist* headers = nullptr;
            if (args.size() > 1 && args[1].isDictionary()) {
                auto dictPtr = args[1].asDictionaryPtr();
                std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                if (dictPtr->getMapCopy().count("insecure") && dictPtr->getMapCopy().at("insecure").isBool() && dictPtr->getMapCopy().at("insecure").asBool()) {
                    ssl_verify = 0L; ssl_verifyhost = 0L;
                }
                for (auto& kv : dictPtr->getMapCopy()) {
                    if (kv.first == "insecure") continue;
                    std::string h = kv.first + ": " + kv.second.toString();
                    headers = curl_slist_append(headers, h.c_str());
                }
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            }
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl_verify);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl_verifyhost);
#ifdef CURLSSLOPT_NATIVE_CA
            curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            CURLcode code = curl_easy_perform(curl);
            if (headers) curl_slist_free_all(headers);
            release_curl_handle(curl);
            if (code != CURLE_OK) { interp.throwException("NetworkError", "http_get failed: " + std::string(curl_easy_strerror(code)), 0, ""); return Value(); }
            return Value(res);
        }));

    interp.defineGlobal("http_post", Value::makeNativeFunction("http_post", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) { interp.throwException("TypeError", "http_post() expects URL and body", 0, ""); return Value(); }
            std::string url = args[0].toString();
            std::string body = args[1].toString();
            CURL* curl = get_curl_handle();
            if (!curl) { interp.throwException("NetworkError", "CURL init failed", 0, ""); return Value(); }
            std::string res;
            long ssl_verify = 1L;
            long ssl_verifyhost = 2L;
            struct curl_slist* headers = nullptr;
            bool hasCT = false;
            if (args.size() > 2 && args[2].isDictionary()) {
                auto dictPtr = args[2].asDictionaryPtr();
                std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                if (dictPtr->getMapCopy().count("insecure") && dictPtr->getMapCopy().at("insecure").isBool() && dictPtr->getMapCopy().at("insecure").asBool()) {
                    ssl_verify = 0L; ssl_verifyhost = 0L;
                }
                for (auto& kv : dictPtr->getMapCopy()) {
                    if (kv.first == "insecure") continue;
                    std::string k = kv.first;
                    std::string h = k + ": " + kv.second.toString();
                    headers = curl_slist_append(headers, h.c_str());
                    if (k == "Content-Type") hasCT = true;
                }
            }
            if (!hasCT && !body.empty() && (body[0] == '{' || body[0] == '[')) {
                headers = curl_slist_append(headers, "Content-Type: application/json");
            }
            if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl_verify); 
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl_verifyhost);
#ifdef CURLSSLOPT_NATIVE_CA
            curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            
            CURLcode code = curl_easy_perform(curl);
            if (headers) curl_slist_free_all(headers);
            release_curl_handle(curl);
            if (code != CURLE_OK) { interp.throwException("NetworkError", "http_post failed: " + std::string(curl_easy_strerror(code)), 0, ""); return Value(); }
            return Value(res);
        }));

    interp.defineGlobal("fetch", Value::makeNativeFunction("fetch", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty()) { interp.throwException("TypeError", "fetch() expects URL", 0, ""); return Value(); }
            std::string url = args[0].toString();
            Value options;
            if (args.size() > 1) options = args[1];
            
            auto ezFut = std::make_shared<EZFuture>();
            EventLoop::instance().retain();
            
            auto makeError = [](const std::string& msg) {
                Value err = Value::makeDictionary();
                err.asDictionaryPtr()->modifyMap([&](auto& m) { m["error"] = Value(msg); });
                return err;
            };

            CURL* curl = curl_easy_init();
            if (!curl) { 
                ezFut->set(makeError("CURL init failed")); 
                EventLoop::instance().release(); 
                return Value::makeFuture(ezFut); 
            }
            
            RequestState* state = new RequestState();
            state->ezFut = ezFut;
            
            std::string method = "GET";
            std::string body;
            long ssl_verify = 1L;
            long ssl_verifyhost = 2L;
            
            if (options.isDictionary()) {
                auto dictPtr = options.asDictionaryPtr();
                std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                const auto& opts = dictPtr->getMapCopy();
                if (opts.count("insecure") && opts.at("insecure").isBool() && opts.at("insecure").asBool()) {
                    ssl_verify = 0L; ssl_verifyhost = 0L;
                }
                if (opts.count("method")) method = opts.at("method").toString();
                if (opts.count("body")) body = opts.at("body").toString();
                if (opts.count("headers") && opts.at("headers").isDictionary()) {
                    auto hDictPtr = opts.at("headers").asDictionaryPtr();
                    std::shared_lock<std::shared_mutex> hLk(hDictPtr->map_mutex);
                    for (const auto& kv : hDictPtr->getMapCopy()) {
                        std::string h = kv.first + ": " + kv.second.toString();
                        state->headers = curl_slist_append(state->headers, h.c_str());
                    }
                }
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state->response);
            curl_easy_setopt(curl, CURLOPT_PRIVATE, state);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl_verify);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl_verifyhost);
#ifdef CURLSSLOPT_NATIVE_CA
            curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
            
            if (method == "POST") {
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, body.c_str());
            } else if (method != "GET") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
                if (!body.empty()) {
                    curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, body.c_str());
                }
            }
            
            if (state->headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, state->headers);
            
            curl_multi_add_handle(g_curl_multi, curl);
            
            return Value::makeFuture(ezFut);
        }));


}
