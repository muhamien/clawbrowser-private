#include "clawbrowser/cli/api_client.h"

#include <utility>

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace clawbrowser {

ApiClient::ApiClient(
    const std::string&,
    const std::string&,
    scoped_refptr<network::SharedURLLoaderFactory>) {}

ApiClient::~ApiClient() = default;

void ApiClient::GenerateFingerprint(const GenerateRequest&,
                                    GenerateCallback callback) {
  std::move(callback).Run(base::unexpected(ApiError{
      0, "local_profile_required",
      "load a validated local fingerprint profile; remote generation is disabled"}));
}

void ApiClient::VerifyProxy(const VerifyProxyRequest&,
                           VerifyCallback callback) {
  std::move(callback).Run(base::unexpected(ApiError{
      0, "external_verification_disabled",
      "proxy verification is limited to local configuration inspection"}));
}

}  // namespace clawbrowser
