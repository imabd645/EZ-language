#include "../Builtins.h"
#include "../RuntimeContext.h"

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

static size_t HttpWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void registerNetBuiltins(RuntimeContext& interp) {
    static int curl_init_checker = []() { curl_global_init(CURL_GLOBAL_DEFAULT); return 0; }();
    (void)curl_init_checker;

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
            if (args.empty()) { interp.runtimeError("http_get() expects URL", 0, ""); return Value(); }
            std::string url = args[0].toString();
            CURL* curl = curl_easy_init();
            if (!curl) { interp.runtimeError("CURL init failed", 0, ""); return Value(); }
            std::string res;
            long ssl_verify = 1L;
            long ssl_verifyhost = 2L;
            struct curl_slist* headers = nullptr;
            if (args.size() > 1 && args[1].isDictionary()) {
                auto dictPtr = args[1].asDictionaryPtr();
                std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                if (dictPtr->map.count("insecure") && dictPtr->map.at("insecure").isBool() && dictPtr->map.at("insecure").asBool()) {
                    ssl_verify = 0L; ssl_verifyhost = 0L;
                }
                for (auto& kv : dictPtr->map) {
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
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            CURLcode code = curl_easy_perform(curl);
            if (headers) curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            if (code != CURLE_OK) { interp.runtimeError("http_get failed: " + std::string(curl_easy_strerror(code)), 0, ""); return Value(); }
            return Value(res);
        }));

    interp.defineGlobal("http_post", Value::makeNativeFunction("http_post", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) { interp.runtimeError("http_post() expects URL and body", 0, ""); return Value(); }
            std::string url = args[0].toString();
            std::string body = args[1].toString();
            CURL* curl = curl_easy_init();
            if (!curl) { interp.runtimeError("CURL init failed", 0, ""); return Value(); }
            std::string res;
            long ssl_verify = 1L;
            long ssl_verifyhost = 2L;
            struct curl_slist* headers = nullptr;
            bool hasCT = false;
            if (args.size() > 2 && args[2].isDictionary()) {
                auto dictPtr = args[2].asDictionaryPtr();
                std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                if (dictPtr->map.count("insecure") && dictPtr->map.at("insecure").isBool() && dictPtr->map.at("insecure").asBool()) {
                    ssl_verify = 0L; ssl_verifyhost = 0L;
                }
                for (auto& kv : dictPtr->map) {
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
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            
            CURLcode code = curl_easy_perform(curl);
            if (headers) curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            if (code != CURLE_OK) { interp.runtimeError("http_post failed: " + std::string(curl_easy_strerror(code)), 0, ""); return Value(); }
            return Value(res);
        }));

    interp.defineGlobal("fetch", Value::makeNativeFunction("fetch", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty()) { interp.runtimeError("fetch() expects URL", 0, ""); return Value(); }
            std::string url = args[0].toString();
            Value options;
            if (args.size() > 1) options = args[1];
            
            std::shared_future<Value> fut = std::async(std::launch::async, 
                [url, options]() -> Value {
                    auto makeError = [](const std::string& msg) {
                        Value err = Value::makeDictionary();
                        err.asDictionaryPtr()->map["error"] = Value(msg);
                        return err;
                    };

                    CURL* curl = curl_easy_init();
                    if (!curl) { return makeError("CURL init failed"); }
                    std::string response;
                    std::string method = "GET";
                    std::string body;
                    struct curl_slist* headers = nullptr;
                    long ssl_verify = 1L;
                    long ssl_verifyhost = 2L;
                    
                    if (options.isDictionary()) {
                        auto dictPtr = options.asDictionaryPtr();
                        std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                        const auto& opts = dictPtr->map;
                        if (opts.count("insecure") && opts.at("insecure").isBool() && opts.at("insecure").asBool()) {
                            ssl_verify = 0L; ssl_verifyhost = 0L;
                        }
                        if (opts.count("method")) method = opts.at("method").toString();
                        if (opts.count("body")) body = opts.at("body").toString();
                        if (opts.count("headers") && opts.at("headers").isDictionary()) {
                            auto hDictPtr = opts.at("headers").asDictionaryPtr();
                            std::shared_lock<std::shared_mutex> hLk(hDictPtr->map_mutex);
                            for (const auto& kv : hDictPtr->map) {
                                std::string h = kv.first + ": " + kv.second.toString();
                                headers = curl_slist_append(headers, h.c_str());
                            }
                        }
                    }

                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpWriteCallback);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl_verify);
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl_verifyhost);
                    
                    if (method == "POST") {
                        curl_easy_setopt(curl, CURLOPT_POST, 1L);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
                    } else if (method != "GET") {
                        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
                    }
                    
                    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                    
                    CURLcode res = curl_easy_perform(curl);
                    if (headers) curl_slist_free_all(headers);
                    curl_easy_cleanup(curl);
                    
                    if (res != CURLE_OK) { return makeError("Fetch failed: " + std::string(curl_easy_strerror(res))); }
                    
                    return Value(response);
                }).share();
                
            return Value::makeFuture(fut);
        }));


}
