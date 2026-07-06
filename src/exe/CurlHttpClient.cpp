#include "qse/exe/CurlHttpClient.h"

#include <curl/curl.h>

#include <mutex>
#include <stdexcept>

namespace {

std::size_t write_body(const char* data, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(data, size * nmemb);
    return size * nmemb;
}

void ensure_curl_global_init() {
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

} // namespace

namespace qse {

CurlHttpClient::CurlHttpClient() {
    ensure_curl_global_init();
}

HttpResponse CurlHttpClient::request(const char* method, const std::string& url,
                                     const std::vector<std::string>& headers,
                                     const std::string* body) {
    HttpResponse response;
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        response.body = "curl_easy_init failed";
        return response;
    }

    curl_slist* header_list = nullptr;
    for (const auto& header : headers) {
        header_list = curl_slist_append(header_list, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (body != nullptr) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body->size()));
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    } else {
        response.status = 0;
        response.body = curl_easy_strerror(rc);
    }

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return response;
}

HttpResponse CurlHttpClient::get(const std::string& url, const std::vector<std::string>& headers) {
    return request("GET", url, headers, nullptr);
}

HttpResponse CurlHttpClient::post(const std::string& url, const std::vector<std::string>& headers,
                                  const std::string& body) {
    return request("POST", url, headers, &body);
}

HttpResponse CurlHttpClient::patch(const std::string& url, const std::vector<std::string>& headers,
                                   const std::string& body) {
    return request("PATCH", url, headers, &body);
}

HttpResponse CurlHttpClient::del(const std::string& url, const std::vector<std::string>& headers) {
    return request("DELETE", url, headers, nullptr);
}

} // namespace qse
