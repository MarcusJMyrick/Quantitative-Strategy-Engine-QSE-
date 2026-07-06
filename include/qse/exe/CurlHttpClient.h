#pragma once

#include "qse/exe/IHttpClient.h"

namespace qse {

/**
 * @brief libcurl-backed IHttpClient (E2). Blocking, 10s timeout per request;
 * transport failures surface as status 0 with the curl error in the body.
 */
class CurlHttpClient : public IHttpClient {
public:
    CurlHttpClient();

    HttpResponse get(const std::string& url, const std::vector<std::string>& headers) override;
    HttpResponse post(const std::string& url, const std::vector<std::string>& headers,
                      const std::string& body) override;
    HttpResponse patch(const std::string& url, const std::vector<std::string>& headers,
                       const std::string& body) override;
    HttpResponse del(const std::string& url, const std::vector<std::string>& headers) override;

private:
    HttpResponse request(const char* method, const std::string& url,
                         const std::vector<std::string>& headers, const std::string* body);
};

} // namespace qse
