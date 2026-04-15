#include "../Builtins.h"
#include "../Interpreter.h"

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

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#endif

static auto HttpWriteCallback = [](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
};

void registerNetBuiltins(Interpreter& interp) {
    static int curl_init_checker = []() { curl_global_init(CURL_GLOBAL_DEFAULT); return 0; }();
    (void)curl_init_checker;

    interp.defineGlobal("url_encode", Value::makeNativeFunction("url_encode", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            std::string s = args[0].toString();
            CURL* curl = curl_easy_init();
            char* output = curl_easy_escape(curl, s.c_str(), (int)s.length());
            std::string res(output);
            curl_free(output);
            curl_easy_cleanup(curl);
            return Value(res);
        }));

    interp.defineGlobal("url_decode", Value::makeNativeFunction("url_decode", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
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
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.empty()) { interp.runtimeError("http_get() expects URL", 0, ""); return Value(); }
            std::string url = args[0].toString();
            CURL* curl = curl_easy_init();
            if (!curl) { interp.runtimeError("CURL init failed", 0, ""); return Value(); }
            std::string res;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t(*)(void*,size_t,size_t,void*))HttpWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            struct curl_slist* headers = nullptr;
            if (args.size() > 1 && args[1].isDictionary()) {
                for (auto& kv : args[1].asDictionary().map) {
                    std::string h = kv.first + ": " + kv.second.toString();
                    headers = curl_slist_append(headers, h.c_str());
                }
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            }
            CURLcode code = curl_easy_perform(curl);
            if (headers) curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            if (code != CURLE_OK) { interp.runtimeError("http_get failed: " + std::string(curl_easy_strerror(code)), 0, ""); return Value(); }
            return Value(res);
        }));

    interp.defineGlobal("http_post", Value::makeNativeFunction("http_post", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) { interp.runtimeError("http_post() expects URL and body", 0, ""); return Value(); }
            std::string url = args[0].toString();
            std::string body = args[1].toString();
            CURL* curl = curl_easy_init();
            if (!curl) { interp.runtimeError("CURL init failed", 0, ""); return Value(); }
            std::string res;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t(*)(void*,size_t,size_t,void*))HttpWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); 
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            
            struct curl_slist* headers = nullptr;
            bool hasCT = false;
            if (args.size() > 2 && args[2].isDictionary()) {
                for (auto& kv : args[2].asDictionary().map) {
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
            CURLcode code = curl_easy_perform(curl);
            if (headers) curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            if (code != CURLE_OK) { interp.runtimeError("http_post failed: " + std::string(curl_easy_strerror(code)), 0, ""); return Value(); }
            return Value(res);
        }));

    interp.defineGlobal("fetch", Value::makeNativeFunction("fetch", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.empty()) { interp.runtimeError("fetch() expects URL", 0, ""); return Value(); }
            std::string url = args[0].toString();
            Value options;
            if (args.size() > 1) options = args[1];
            
            std::shared_future<Value> fut = std::async(std::launch::async, 
                [url, options, &interp]() -> Value {
                    CURL* curl = curl_easy_init();
                    if (!curl) { interp.runtimeError("CURL init failed", 0, ""); return Value(); }
                    std::string response;
                    std::string method = "GET";
                    std::string body;
                    struct curl_slist* headers = nullptr;
                    
                    if (options.isDictionary()) {
                        const auto& opts = options.asDictionary().map;
                        if (opts.count("method")) method = opts.at("method").toString();
                        if (opts.count("body")) body = opts.at("body").toString();
                        if (opts.count("headers") && opts.at("headers").isDictionary()) {
                            for (const auto& kv : opts.at("headers").asDictionary().map) {
                                std::string h = kv.first + ": " + kv.second.toString();
                                headers = curl_slist_append(headers, h.c_str());
                            }
                        }
                    }

                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t(*)(void*,size_t,size_t,void*))HttpWriteCallback);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
                    
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
                    
                    if (res != CURLE_OK) { interp.runtimeError("Fetch failed: " + std::string(curl_easy_strerror(res)), 0, ""); return Value(); }
                    
                    return Value(response);
                }).share();
                
            return Value::makeFuture(fut);
        }));

#ifdef _WIN32
    interp.defineGlobal("server", Value::makeNativeFunction("server", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("server() port must be a number", 0, ""); return Value(); }
            if (!args[1].isFunction()) { interp.runtimeError("server() handler must be a function", 0, ""); return Value(); }
            
            int port = static_cast<int>(args[0].asNumber());
            Value handler = args[1];

            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { interp.runtimeError("WSAStartup failed", 0, ""); return Value(); }

            SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenSocket == INVALID_SOCKET) { WSACleanup(); interp.runtimeError("Socket creation failed", 0, ""); return Value(); }

            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = INADDR_ANY;
            serverAddr.sin_port = htons(port);

            if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                closesocket(listenSocket); WSACleanup(); interp.runtimeError("Bind failed", 0, ""); return Value();
            }

            if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
                closesocket(listenSocket); WSACleanup(); interp.runtimeError("Listen failed", 0, ""); return Value();
            }

            while (true) {
                SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
                if (clientSocket == INVALID_SOCKET) break;

                auto globalEnv = interp.getGlobalEnv();
                
                std::thread([clientSocket, handler, globalEnv]() {
                    auto requestEnv = globalEnv->createChild();
                    Interpreter threadInterp(requestEnv);

                    std::string request;
                    char buffer[4096];
                    bool headersComplete = false;
                    size_t contentLen = 0;
                    
                    while (!headersComplete) {
                        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
                        if (bytesRead <= 0) break;
                        request.append(buffer, bytesRead);
                        
                        size_t headerEnd = request.find("\r\n\r\n");
                        if (headerEnd != std::string::npos) {
                            headersComplete = true;
                            size_t clPos = request.find("Content-Length: ");
                            if (clPos != std::string::npos) {
                                size_t start = clPos + 16;
                                size_t end = request.find("\r\n", start);
                                if (end != std::string::npos) contentLen = std::stoi(request.substr(start, end - start));
                            }
                        }
                    }

                    if (headersComplete) {
                        size_t headerEnd = request.find("\r\n\r\n");
                        size_t bodyStart = headerEnd + 4;
                        
                        while (request.length() - bodyStart < contentLen) {
                            int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
                            if (bytesRead <= 0) break;
                            request.append(buffer, bytesRead);
                        }
                        
                        std::string method, fullPath, version, body;
                        std::unordered_map<std::string, Value> headers, query;

                        size_t firstSpace = request.find(' ');
                        if (firstSpace != std::string::npos) {
                            method = request.substr(0, firstSpace);
                            size_t secondSpace = request.find(' ', firstSpace + 1);
                            if (secondSpace != std::string::npos) {
                                fullPath = request.substr(firstSpace + 1, secondSpace - (firstSpace + 1));
                                version = request.substr(secondSpace + 1, headerEnd - (secondSpace + 1)); 
                            }
                        }

                        std::string path = fullPath;
                        size_t qPos = fullPath.find('?');
                        if (qPos != std::string::npos) {
                            path = fullPath.substr(0, qPos);
                            std::string qStr = fullPath.substr(qPos + 1);
                            size_t start = 0;
                            while (start < qStr.length()) {
                                size_t amPos = qStr.find('&', start);
                                std::string pair = qStr.substr(start, amPos == std::string::npos ? amPos : amPos - start);
                                size_t eqPos = pair.find('=');
                                if (eqPos != std::string::npos) { query[pair.substr(0, eqPos)] = Value(pair.substr(eqPos + 1)); } 
                                else if (!pair.empty()) { query[pair] = Value(true); }
                                if (amPos == std::string::npos) break;
                                start = amPos + 1;
                            }
                        }
                        
                        if (request.length() > bodyStart) body = request.substr(bodyStart);
                        
                        size_t pos = request.find("\r\n") + 2;
                        while (pos < headerEnd) {
                            size_t nextLine = request.find("\r\n", pos);
                            if (nextLine == std::string::npos || nextLine > headerEnd) break;
                            std::string line = request.substr(pos, nextLine - pos);
                            size_t colon = line.find(':');
                            if (colon != std::string::npos) {
                                std::string k = line.substr(0, colon);
                                std::string v = line.substr(colon + 1);
                                v.erase(0, v.find_first_not_of(" "));
                                headers[k] = Value(v);
                            }
                            pos = nextLine + 2;
                        }

                        std::unordered_map<std::string, Value> formData;
                        if (headers.count("Content-Type") && headers["Content-Type"].asString().find("application/x-www-form-urlencoded") != std::string::npos) {
                            std::string qStr = body;
                            size_t start = 0;
                            while (start < qStr.length()) {
                                size_t amPos = qStr.find('&', start);
                                std::string pair = qStr.substr(start, amPos == std::string::npos ? amPos : amPos - start);
                                size_t eqPos = pair.find('=');
                                if (eqPos != std::string::npos) {
                                    std::string key = pair.substr(0, eqPos);
                                    std::string val = pair.substr(eqPos + 1);
                                    std::replace(key.begin(), key.end(), '+', ' ');
                                    std::replace(val.begin(), val.end(), '+', ' ');
                                    CURL* curl = curl_easy_init();
                                    if (curl) {
                                        int outlen;
                                        char* uns_key = curl_easy_unescape(curl, key.c_str(), (int)key.length(), &outlen);
                                        std::string dec_key(uns_key, outlen);
                                        curl_free(uns_key);
                                        char* uns_val = curl_easy_unescape(curl, val.c_str(), (int)val.length(), &outlen);
                                        std::string dec_val(uns_val, outlen);
                                        curl_free(uns_val);
                                        curl_easy_cleanup(curl);
                                        formData[dec_key] = Value(dec_val);
                                    }
                                }
                                if (amPos == std::string::npos) break;
                                start = amPos + 1;
                            }
                        }

                        Value reqArg = Value::makeDictionary();
                        auto& reqMap = reqArg.asDictionary().map;
                        reqMap["method"] = Value(method);
                        reqMap["path"] = Value(path);
                        reqMap["fullPath"] = Value(fullPath);
                        reqMap["version"] = Value(version);
                        reqMap["body"] = Value(body);
                        
                        Value formDict = Value::makeDictionary(); formDict.asDictionary().map = std::move(formData); reqMap["form"] = formDict;
                        Value queryDict = Value::makeDictionary(); queryDict.asDictionary().map = std::move(query); reqMap["query"] = queryDict;
                        Value headerDict = Value::makeDictionary(); headerDict.asDictionary().map = std::move(headers); reqMap["headers"] = headerDict;

                        std::vector<Value> callbackArgs = {reqArg};
                        try {
                            Value result = threadInterp.callFunction(handler, callbackArgs, 0, "native");
                            std::string respStr;
                            
                            if (result.isDictionary()) {
                                auto& d = result.asDictionary().map;
                                int status = d.count("status") ? (int)d.at("status").asNumber() : 200;
                                std::string b = d.count("body") ? d.at("body").toString() : "";
                                
                                respStr = "HTTP/1.1 " + std::to_string(status) + " OK\r\n";
                                if (d.count("headers") && d.at("headers").isDictionary()) {
                                    for (auto& kv : d.at("headers").asDictionary().map) { respStr += kv.first + ": " + kv.second.toString() + "\r\n"; }
                                } else { respStr += "Content-Type: text/html\r\n"; }
                                respStr += "Content-Length: " + std::to_string(b.length()) + "\r\n\r\n" + b;
                            } else {
                                respStr = result.toString();
                                if (respStr.find("HTTP/") != 0) {
                                    std::string b = respStr;
                                    respStr = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(b.length()) + "\r\n\r\n" + b;
                                }
                            }
                            send(clientSocket, respStr.c_str(), (int)respStr.length(), 0);
                        } catch (const std::exception& e) {
                            std::string errResp = "HTTP/1.1 500 Internal Server Error\r\n\r\nServer Error: " + std::string(e.what());
                            send(clientSocket, errResp.c_str(), (int)errResp.length(), 0);
                        }
                    }
                    closesocket(clientSocket);
                }).detach();
            }

            closesocket(listenSocket);
            WSACleanup();
            return Value();
        }));

    interp.defineGlobal("crypto_hash", Value::makeNativeFunction("crypto_hash", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            std::string algo = args[0].asString();
            std::string text = args[1].asString();
            HCRYPTPROV hProv = 0;
            HCRYPTHASH hHash = 0;
            ALG_ID algId = CALG_SHA_256;
            if (algo == "md5") algId = CALG_MD5;
            else if (algo == "sha1") algId = CALG_SHA1;
            else if (algo == "sha256") algId = CALG_SHA_256;
            else { interp.runtimeError("Unknown hash algorithm", 0, ""); return Value(); }
            
            if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return Value("Err Context: " + std::to_string(GetLastError()));
            if (!CryptCreateHash(hProv, algId, 0, 0, &hHash)) { CryptReleaseContext(hProv, 0); return Value("Err Hash: " + std::to_string(GetLastError())); }
            CryptHashData(hHash, (BYTE*)text.c_str(), text.length(), 0);
            
            DWORD hashLen = 0;
            DWORD bCount = sizeof(DWORD);
            CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &bCount, 0);
            std::vector<BYTE> hash(hashLen);
            CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &hashLen, 0);
            
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            
            std::stringstream ss;
            for (size_t i = 0; i < hash.size(); i++) { ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i]; }
            return Value(ss.str());
        }));

    interp.defineGlobal("crypto_base64_encode", Value::makeNativeFunction("crypto_base64_encode", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            std::string text = args[0].asString();
            DWORD size = 0;
            CryptBinaryToStringA((const BYTE*)text.c_str(), text.length(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &size);
            std::string out(size, '\0');
            CryptBinaryToStringA((const BYTE*)text.c_str(), text.length(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &size);
            while(!out.empty() && out.back() == '\0') out.pop_back();
            return Value(out);
        }));

    interp.defineGlobal("crypto_base64_decode", Value::makeNativeFunction("crypto_base64_decode", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            std::string text = args[0].asString();
            DWORD size = 0;
            CryptStringToBinaryA(text.c_str(), text.length(), CRYPT_STRING_BASE64, NULL, &size, NULL, NULL);
            std::string out(size, '\0');
            CryptStringToBinaryA(text.c_str(), text.length(), CRYPT_STRING_BASE64, (BYTE*)&out[0], &size, NULL, NULL);
            return Value(out);
        }));

    auto crypto_aes = [](Interpreter& interp, const std::string& text, const std::string& key, bool isEncrypt) -> Value {
        HCRYPTPROV hProv;
        HCRYPTHASH hHash;
        HCRYPTKEY hKey;
        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return Value("Err Context AES: " + std::to_string(GetLastError()));
        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) { CryptReleaseContext(hProv, 0); return Value("Err Hash AES: " + std::to_string(GetLastError())); }
        CryptHashData(hHash, (BYTE*)key.c_str(), key.length(), 0);
        if (!CryptDeriveKey(hProv, CALG_AES_256, hHash, 0, &hKey)) { CryptDestroyHash(hHash); CryptReleaseContext(hProv, 0); return Value("Err Derive AES: " + std::to_string(GetLastError())); }
        
        std::vector<BYTE> data(text.begin(), text.end());
        DWORD dataLen = data.size();
        
        if (isEncrypt) {
            DWORD bufLen = dataLen + 64; 
            data.resize(bufLen);
            if (CryptEncrypt(hKey, 0, TRUE, 0, data.data(), &dataLen, bufLen)) { data.resize(dataLen); } else { data.clear(); }
        } else {
            if (CryptDecrypt(hKey, 0, TRUE, 0, data.data(), &dataLen)) { data.resize(dataLen); } else { data.clear(); }
        }
       
       CryptDestroyKey(hKey);
       CryptDestroyHash(hHash);
       CryptReleaseContext(hProv, 0);
       return Value(std::string(data.begin(), data.end()));
    };

    interp.defineGlobal("crypto_encrypt", Value::makeNativeFunction("crypto_encrypt", 2,
        [crypto_aes](Interpreter& interp, const std::vector<Value>& args) -> Value {
            return crypto_aes(interp, args[0].asString(), args[1].asString(), true);
        }));
        
    interp.defineGlobal("crypto_decrypt", Value::makeNativeFunction("crypto_decrypt", 2,
        [crypto_aes](Interpreter& interp, const std::vector<Value>& args) -> Value {
            return crypto_aes(interp, args[0].asString(), args[1].asString(), false);
        }));

#else
    interp.defineGlobal("server", Value::makeNativeFunction("server", 2,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            interp.runtimeError("server() is only supported on Windows", 0, ""); return Value();
         }));
#endif

    interp.defineGlobal("serveFile", Value::makeNativeFunction("serveFile", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("serveFile() expects string path", 0, ""); return Value(); }
            std::string path = args[0].asString();
            
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) {
                Value resp = Value::makeDictionary();
                resp.asDictionary().map["status"] = Value(404.0);
                resp.asDictionary().map["body"] = Value("File not found: " + path);
                return resp;
            }
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string body = buffer.str();
            
            std::string ext = "";
            size_t dot = path.find_last_of('.');
            if (dot != std::string::npos) ext = path.substr(dot + 1);
            
            std::string mime = "text/plain";
            if (ext == "html" || ext == "htm") mime = "text/html";
            else if (ext == "css") mime = "text/css";
            else if (ext == "js") mime = "text/javascript";
            else if (ext == "png") mime = "image/png";
            else if (ext == "jpg" || ext == "jpeg") mime = "image/jpeg";
            else if (ext == "json") mime = "application/json";
            
            Value resp = Value::makeDictionary();
            auto& d = resp.asDictionary().map;
            d["status"] = Value(200.0);
            
            Value headers = Value::makeDictionary();
            headers.asDictionary().map["Content-Type"] = Value(mime);
            d["headers"] = headers;
            
            d["body"] = Value(body);
            return resp;
        }));
}
