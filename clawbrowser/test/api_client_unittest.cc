#include "clawbrowser/cli/api_client.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(LocalApiClientTest, GenerationFailsWithoutNetworkRequest) {
  base::test::TaskEnvironment environment;
  network::TestURLLoaderFactory factory;
  ApiClient client("https://forbidden.invalid", "ignored",
                   factory.GetSafeWeakWrapper());
  base::test::TestFuture<base::expected<GenerateResponse, ApiError>> future;
  client.GenerateFingerprint(GenerateRequest(), future.GetCallback());
  ASSERT_TRUE(future.IsReady());
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error().code, "local_profile_required");
  EXPECT_EQ(factory.NumPending(), 0);
}

TEST(LocalApiClientTest, VerificationFailsWithoutNetworkRequest) {
  base::test::TaskEnvironment environment;
  network::TestURLLoaderFactory factory;
  ApiClient client("https://forbidden.invalid", "ignored",
                   factory.GetSafeWeakWrapper());
  base::test::TestFuture<base::expected<VerifyProxyResponse, ApiError>> future;
  client.VerifyProxy(VerifyProxyRequest(), future.GetCallback());
  ASSERT_TRUE(future.IsReady());
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error().code, "external_verification_disabled");
  EXPECT_EQ(factory.NumPending(), 0);
}

}  // namespace
}  // namespace clawbrowser
