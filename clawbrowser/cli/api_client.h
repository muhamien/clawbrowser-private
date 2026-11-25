#ifndef CLAWBROWSER_CLI_API_CLIENT_H_
#define CLAWBROWSER_CLI_API_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "clawbrowser/generated/fingerprint_types.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace clawbrowser {

struct ApiError {
  int http_status = 0;  // 0 = network error
  std::string code;
  std::string message;
};

// Compatibility boundary for inherited callers. All vendor operations fail
// locally, without constructing a network request.
class ApiClient {
 public:
  ApiClient(const std::string& base_url,
            const std::string& api_key,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ApiClient();

  using GenerateCallback =
      base::OnceCallback<void(base::expected<GenerateResponse, ApiError>)>;
  using VerifyCallback =
      base::OnceCallback<void(base::expected<VerifyProxyResponse, ApiError>)>;

  void GenerateFingerprint(const GenerateRequest& params,
                           GenerateCallback callback);

  void VerifyProxy(const VerifyProxyRequest& request,
                   VerifyCallback callback);

};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_CLI_API_CLIENT_H_
