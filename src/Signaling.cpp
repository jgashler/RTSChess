#include "Signaling.h"
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <functional>
#include <cstring>

static const char* WORKER_HOST = "rtschess-signaling.jgashler44.workers.dev";
// ─────────────────────────────────────────────────────────────────────────────

// ── Platform HTTP ─────────────────────────────────────────────────────────────

struct HttpResp { int status = 0; std::string body; };

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
#  define NOMINMAX
#  include <windows.h>
#  include <winhttp.h>

static HttpResp httpRequest(const char* method, const char* path,
                             const std::string& body = {}) {
    HttpResp result;

    // Convert narrow strings to wide for WinHTTP
    auto toWide = [](const char* s) -> std::wstring {
        int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
        std::wstring w(n, 0);
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
        return w;
    };

    HINTERNET hSess = WinHttpOpen(L"RTSChess/1.0",
                                  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return result;

    HINTERNET hConn = WinHttpConnect(hSess, toWide(WORKER_HOST).c_str(),
                                     INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return result; }

    auto wPath   = toWide(path);
    auto wMethod = toWide(method);
    HINTERNET hReq = WinHttpOpenRequest(hConn, wMethod.c_str(), wPath.c_str(),
                                        nullptr, WINHTTP_NO_REFERER,
                                        WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (!hReq) {
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSess);
        return result;
    }

    BOOL sent;
    if (!body.empty()) {
        sent = WinHttpSendRequest(hReq,
            L"Content-Type: text/plain\r\n", (DWORD)-1L,
            (LPVOID)body.c_str(), (DWORD)body.size(),
            (DWORD)body.size(), 0);
    } else {
        sent = WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    }
    if (sent) WinHttpReceiveResponse(hReq, nullptr);

    DWORD statusCode = 0, sz = sizeof(statusCode);
    WinHttpQueryHeaders(hReq,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &sz,
        WINHTTP_NO_HEADER_INDEX);
    result.status = (int)statusCode;

    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
        std::string chunk(avail, '\0');
        DWORD read = 0;
        WinHttpReadData(hReq, chunk.data(), avail, &read);
        result.body.append(chunk.data(), read);
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return result;
}

#else  // macOS / Linux — use system libcurl

#  include <curl/curl.h>

static size_t curlWrite(char* ptr, size_t sz, size_t n, std::string* s) {
    s->append(ptr, sz * n);
    return sz * n;
}

static HttpResp httpRequest(const char* method, const char* path,
                             const std::string& body = {}) {
    HttpResp result;
    CURL* curl = curl_easy_init();
    if (!curl) return result;

    std::string url = std::string("https://") + WORKER_HOST + path;
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &result.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       10L);

    struct curl_slist* headers = nullptr;
    if (!body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POST,          1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        headers = curl_slist_append(headers, "Content-Type: text/plain");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    curl_easy_perform(curl);

    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    result.status = (int)code;

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}
#endif

// ── Main-thread dispatch ───────────────────────────────────────────────────

static std::mutex                        g_mtx;
static std::vector<std::function<void()>> g_pending;

static void push(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(g_mtx);
    g_pending.push_back(std::move(fn));
}

void Signaling::Poll() {
    std::vector<std::function<void()>> local;
    { std::lock_guard<std::mutex> lk(g_mtx); local = std::move(g_pending); }
    for (auto& fn : local) fn();
}

// ── Public API ────────────────────────────────────────────────────────────────

void Signaling::AsyncPostOffer(const std::string& offer,
                                std::function<void(std::string)> cb) {
    std::thread([offer, cb]() {
        auto r = httpRequest("POST", "/host", offer);
        std::string code = (r.status == 200) ? r.body : "";
        push([cb, code]() { cb(code); });
    }).detach();
}

void Signaling::AsyncGetOffer(const std::string& code,
                               std::function<void(std::string)> cb) {
    std::thread([code, cb]() {
        std::string path = "/join/" + code;
        auto r = httpRequest("GET", path.c_str());
        std::string offer = (r.status == 200) ? r.body : "";
        push([cb, offer]() { cb(offer); });
    }).detach();
}

void Signaling::AsyncPostAnswer(const std::string& code,
                                 const std::string& answer,
                                 std::function<void(bool)> cb) {
    std::thread([code, answer, cb]() {
        std::string path = "/answer/" + code;
        auto r = httpRequest("POST", path.c_str(), answer);
        bool ok = (r.status == 200);
        push([cb, ok]() { cb(ok); });
    }).detach();
}

void Signaling::AsyncPollAnswer(const std::string& code,
                                 std::function<void(std::string)> cb) {
    std::thread([code, cb]() {
        std::string path = "/answer/" + code;
        auto r = httpRequest("GET", path.c_str());
        std::string answer = (r.status == 200) ? r.body : "";
        push([cb, answer]() { cb(answer); });
    }).detach();
}
