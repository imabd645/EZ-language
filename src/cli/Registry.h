#ifndef EZ_REGISTRY_H
#define EZ_REGISTRY_H

// ============================================================================
// Registry client: HTTP, credentials and configuration.
//
// Everything the package commands need to talk to an ez-registry instance. The
// registry does dependency resolution server-side, so this side only has to
// ask, verify what comes back, and unpack it.
// ============================================================================

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <curl/curl.h>
#include <openssl/evp.h>
#include "utils/MiniJson.h"

namespace fs = std::filesystem;

namespace ezreg {

inline const char* DEFAULT_REGISTRY = "https://packages.ez-lang.site";

// ── Paths ───────────────────────────────────────────────────────────────────

inline std::string homeDir() {
#ifdef _WIN32
    const char* h = std::getenv("USERPROFILE");
    if (h && *h) return h;
    const char* drive = std::getenv("HOMEDRIVE");
    const char* path = std::getenv("HOMEPATH");
    if (drive && path) return std::string(drive) + path;
#else
    const char* h = std::getenv("HOME");
    if (h && *h) return h;
#endif
    return ".";
}

inline std::string ezHome() {
    std::string dir = homeDir() + "/.ez";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

inline std::string configPath()      { return ezHome() + "/config.json"; }
inline std::string credentialsPath() { return ezHome() + "/credentials.json"; }
inline std::string cacheDir() {
    std::string dir = ezHome() + "/cache";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// ── Small JSON helpers ──────────────────────────────────────────────────────

inline MiniJson::Value parseJson(const std::string& text, bool& ok) {
    MiniJson::Value root;
    MiniJson::Reader reader;
    ok = reader.parse(text, root);
    return root;
}

inline std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

inline std::string readFileText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline bool writeFileText(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << text;
    return f.good();
}

// ── Configuration ───────────────────────────────────────────────────────────

inline std::string trimSlash(std::string url) {
    while (!url.empty() && (url.back() == '/' || url.back() == ' ')) url.pop_back();
    return url;
}

/** Registry URL: EZ_REGISTRY env var, then ~/.ez/config.json, then the default. */
inline std::string registryUrl() {
    const char* env = std::getenv("EZ_REGISTRY");
    if (env && *env) return trimSlash(env);
    bool ok = false;
    MiniJson::Value cfg = parseJson(readFileText(configPath()), ok);
    if (ok && cfg.properties.count("registry")) {
        std::string v = cfg["registry"].asString();
        if (!v.empty()) return trimSlash(v);
    }
    return DEFAULT_REGISTRY;
}

inline bool setRegistryUrl(const std::string& url) {
    return writeFileText(configPath(), "{\n  \"registry\": \"" + jsonEscape(trimSlash(url)) + "\"\n}\n");
}

/** Stored per registry, so tokens for one host are never sent to another. */
inline std::string storedToken(const std::string& registry) {
    bool ok = false;
    MiniJson::Value creds = parseJson(readFileText(credentialsPath()), ok);
    if (!ok) return "";
    if (creds.properties.count(registry)) {
        MiniJson::Value entry = creds[registry];
        if (entry.properties.count("token")) return entry["token"].asString();
    }
    return "";
}

inline bool storeToken(const std::string& registry, const std::string& token, const std::string& username) {
    // Read-modify-write so logging into a second registry does not drop the first.
    bool ok = false;
    MiniJson::Value creds = parseJson(readFileText(credentialsPath()), ok);
    std::string out = "{\n";
    bool first = true;
    if (ok) {
        for (const auto& key : creds.getMemberNames()) {
            if (key == registry) continue;
            MiniJson::Value e = creds[key];
            if (!first) out += ",\n";
            out += "  \"" + jsonEscape(key) + "\": {\"token\": \"" +
                   jsonEscape(e.get("token", "").asString()) + "\", \"username\": \"" +
                   jsonEscape(e.get("username", "").asString()) + "\"}";
            first = false;
        }
    }
    if (!token.empty()) {
        if (!first) out += ",\n";
        out += "  \"" + jsonEscape(registry) + "\": {\"token\": \"" + jsonEscape(token) +
               "\", \"username\": \"" + jsonEscape(username) + "\"}";
    }
    out += "\n}\n";

    if (!writeFileText(credentialsPath(), out)) return false;
#ifndef _WIN32
    // The file holds a publish credential; keep it out of other users' reach.
    std::error_code ec;
    fs::permissions(credentialsPath(), fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace, ec);
#endif
    return true;
}

// ── HTTP ────────────────────────────────────────────────────────────────────

struct Response {
    long status = 0;
    std::string body;
    std::string error;
    bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

/**
 * Certificate trust for a statically linked curl on Windows.
 *
 * A static build has no baked-in CA bundle and there is no /etc/ssl/certs to
 * fall back on, so verification fails outright with "Problem with the SSL CA
 * cert". CURLSSLOPT_NATIVE_CA points curl at the Windows certificate store,
 * which is the trust root the machine already maintains.
 *
 * The alternative some clients reach for -- switching verification off -- would
 * mean any network attacker could serve a modified package over a connection
 * that still looks encrypted. Package installs run arbitrary code, so that is
 * not a tradeoff worth making; EZ_CA_BUNDLE is offered for the unusual case of
 * a corporate proxy whose root is not in the store.
 */
inline void applyTlsTrust(CURL* curl) {
    const char* bundle = std::getenv("EZ_CA_BUNDLE");
    if (bundle && *bundle) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, bundle);
        return;
    }
#ifdef CURLSSLOPT_NATIVE_CA
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, (long)CURLSSLOPT_NATIVE_CA);
#endif
}

inline size_t writeToString(void* data, size_t size, size_t nmemb, void* userp) {
    static_cast<std::string*>(userp)->append(static_cast<char*>(data), size * nmemb);
    return size * nmemb;
}

class Http {
public:
    static Response get(const std::string& url, const std::string& bearer = "") {
        return perform(url, "GET", "", "", bearer, nullptr);
    }

    static Response postJson(const std::string& url, const std::string& json,
                             const std::string& bearer = "") {
        return perform(url, "POST", json, "application/json", bearer, nullptr);
    }

    static Response del(const std::string& url, const std::string& bearer = "") {
        return perform(url, "DELETE", "", "", bearer, nullptr);
    }

    /** Multipart publish: a JSON manifest field plus the tarball file. */
    static Response publish(const std::string& url, const std::string& manifestJson,
                            const std::string& tarballPath, const std::string& readme,
                            const std::string& bearer) {
        CURL* curl = curl_easy_init();
        if (!curl) return {0, "", "could not initialise curl"};

        curl_mime* mime = curl_mime_init(curl);
        curl_mimepart* part = curl_mime_addpart(mime);
        curl_mime_name(part, "manifest");
        curl_mime_data(part, manifestJson.c_str(), CURL_ZERO_TERMINATED);

        if (!readme.empty()) {
            part = curl_mime_addpart(mime);
            curl_mime_name(part, "readme");
            curl_mime_data(part, readme.c_str(), CURL_ZERO_TERMINATED);
        }

        part = curl_mime_addpart(mime);
        curl_mime_name(part, "tarball");
        curl_mime_filedata(part, tarballPath.c_str());

        Response r = perform(url, "POST", "", "", bearer, mime);
        curl_mime_free(mime);
        return r;
    }

    /** Download to a file. Returns false and leaves nothing behind on failure. */
    static bool download(const std::string& url, const std::string& outPath, std::string& error) {
        CURL* curl = curl_easy_init();
        if (!curl) { error = "could not initialise curl"; return false; }

        std::string tmp = outPath + ".part";
        FILE* fp = std::fopen(tmp.c_str(), "wb");
        if (!fp) {
            curl_easy_cleanup(curl);
            error = "cannot write to " + tmp;
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        applyTlsTrust(curl);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "ez-cli");
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 128L);

        CURLcode rc = curl_easy_perform(curl);
        std::fclose(fp);
        curl_easy_cleanup(curl);

        if (rc != CURLE_OK) {
            std::error_code ec;
            fs::remove(tmp, ec);
            error = curl_easy_strerror(rc);
            return false;
        }
        std::error_code ec;
        fs::remove(outPath, ec);
        fs::rename(tmp, outPath, ec);
        if (ec) { error = "could not finalise " + outPath; return false; }
        return true;
    }

private:
    static Response perform(const std::string& url, const std::string& method,
                            const std::string& body, const std::string& contentType,
                            const std::string& bearer, curl_mime* mime) {
        Response r;
        CURL* curl = curl_easy_init();
        if (!curl) { r.error = "could not initialise curl"; return r; }

        struct curl_slist* headers = nullptr;
        if (!contentType.empty()) {
            headers = curl_slist_append(headers, ("Content-Type: " + contentType).c_str());
        }
        if (!bearer.empty()) {
            headers = curl_slist_append(headers, ("Authorization: Bearer " + bearer).c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        applyTlsTrust(curl);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r.body);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "ez-cli");
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
        if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        if (mime) {
            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        } else if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        } else if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }

        CURLcode rc = curl_easy_perform(curl);
        if (rc != CURLE_OK) {
            r.error = curl_easy_strerror(rc);
        } else {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status);
        }
        if (headers) curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return r;
    }
};

/** The `error` field of a registry response, or a sensible fallback. */
inline std::string errorMessage(const Response& r) {
    if (!r.error.empty()) return r.error;
    bool ok = false;
    MiniJson::Value body = parseJson(r.body, ok);
    if (ok && body.properties.count("error")) {
        std::string msg = body["error"].asString();
        if (body.properties.count("detail")) {
            MiniJson::Value d = body["detail"];
            if (d.properties.count("required")) {
                msg += "\n  required by:";
                for (const auto& item : d["required"].items) {
                    msg += "\n    " + item.get("by", "?").asString() +
                           " wants " + item.get("range", "?").asString();
                }
            }
            if (d.properties.count("available")) {
                msg += "\n  available:";
                for (const auto& item : d["available"].items) msg += " " + item.asString();
            }
        }
        return msg;
    }
    return "HTTP " + std::to_string(r.status);
}

// ── Integrity ───────────────────────────────────────────────────────────────

/** Lowercase hex SHA-256 of a file, or "" if it cannot be read. */
inline std::string sha256File(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    std::vector<char> buf(64 * 1024);
    while (f.read(buf.data(), static_cast<std::streamsize>(buf.size())) || f.gcount() > 0) {
        EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(f.gcount()));
        if (f.eof()) break;
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, digest, &len);
    EVP_MD_CTX_free(ctx);

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (unsigned int i = 0; i < len; ++i) {
        out += hex[(digest[i] >> 4) & 0xF];
        out += hex[digest[i] & 0xF];
    }
    return out;
}

} // namespace ezreg

#endif
