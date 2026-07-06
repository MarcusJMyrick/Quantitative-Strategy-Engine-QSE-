#pragma once

#include <string>
#include <vector>

namespace qse {

struct HttpResponse {
    long status = 0; // 0 = transport failure (body carries the error text)
    std::string body;

    bool ok() const { return status >= 200 && status < 300; }
};

/**
 * @brief Minimal HTTP client seam (E2). AlpacaExecutionHandler talks to this
 * interface so unit tests inject a mock and CI never touches the network;
 * CurlHttpClient is the production implementation.
 */
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    virtual HttpResponse get(const std::string& url, const std::vector<std::string>& headers) = 0;
    virtual HttpResponse post(const std::string& url, const std::vector<std::string>& headers,
                              const std::string& body) = 0;
    virtual HttpResponse patch(const std::string& url, const std::vector<std::string>& headers,
                               const std::string& body) = 0;
    virtual HttpResponse del(const std::string& url, const std::vector<std::string>& headers) = 0;
};

} // namespace qse
