#include "clawbrowser/startup.h"

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/cli/profile_manager.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/fingerprint_loader.h"
#include "clawbrowser/logging.h"
#include "clawbrowser/profile_envelope.h"
#include "components/version_info/version_info.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#endif
#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "clawbrowser/font_catalog_identity.h"
#endif

namespace clawbrowser {
namespace {

constexpr char kConfiguredApiBaseUrl[] = "http://127.0.0.1:8787";

class StartupTest : public testing::Test {
 protected:
#if BUILDFLAG(IS_MAC)
  static base::ScopedTempDir& CatalogBundle() {
    static base::NoDestructor<base::ScopedTempDir> bundle;
    return *bundle;
  }
  static void SetUpTestSuite() {
    ASSERT_TRUE(CatalogBundle().CreateUniqueTempDir());
    base::FilePath executable_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_dir));
    const auto resources = CatalogBundle().GetPath().AppendASCII("Resources");
    ASSERT_TRUE(base::CreateDirectory(resources));
    ASSERT_TRUE(base::CopyDirectory(
        executable_dir.AppendASCII("clawbrowser-fonts"),
        resources.AppendASCII("clawbrowser-fonts"), true));
  }
  static void TearDownTestSuite() { EXPECT_TRUE(CatalogBundle().Delete()); }
#endif
  static std::string HttpStatusLine(net::HttpStatusCode status) {
    switch (status) {
      case net::HTTP_UNAUTHORIZED:
        return "401 Unauthorized";
      case net::HTTP_FORBIDDEN:
        return "403 Forbidden";
      case net::HTTP_INTERNAL_SERVER_ERROR:
        return "500 Internal Server Error";
      default:
        return base::StringPrintf("%d", static_cast<int>(status));
    }
  }

  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    base::apple::SetOverrideFrameworkBundlePath(CatalogBundle().GetPath());
#endif
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_WIN)
    local_app_data_override_ =
        std::make_unique<base::ScopedPathOverride>(base::DIR_LOCAL_APP_DATA,
                                                   temp_dir_.GetPath());
#else
    home_override_ =
        std::make_unique<base::ScopedPathOverride>(base::DIR_HOME,
                                                   temp_dir_.GetPath());
#endif
    // Keep environment-based path fallbacks inside the test temp dir.
    env_ = base::Environment::Create();
    env_->SetVar("HOME", temp_dir_.GetPath().AsUTF8Unsafe());
    // Create config dir structure
    base::CreateDirectory(GetConfigDir());
  }

  void TearDown() override {
#if BUILDFLAG(IS_MAC)
    base::apple::SetOverrideFrameworkBundlePath(base::FilePath());
#endif
    FingerprintAccessor::Reset();
    env_->UnSetVar("HOME");
    env_->UnSetVar("CLAWBROWSER_API_KEY");
    env_->UnSetVar("CLAWBROWSER_API_BASE_URL");
    env_->UnSetVar("CLAWBROWSER_DEFAULT_FINGERPRINT_ID");
    env_->UnSetVar("CLAWBROWSER_DEV_PROXY_URL");
    env_->UnSetVar("CLAWBROWSER_RUNTIME_GPU");
  }

  void WriteConfigJson(const std::string& api_key,
                       const std::string& base_url = "") {
    base::FilePath config_path = GetConfigDir().AppendASCII("config.json");
    std::string json = "{\"api_key\": \"" + api_key + "\"";
    if (!base_url.empty()) {
      json += ", \"api_base_url\": \"" + base_url + "\"";
    }
    json += "}";
    base::WriteFile(config_path, json);
  }

  base::FilePath GetConfigDir() const {
#if BUILDFLAG(IS_WIN)
    return temp_dir_.GetPath().AppendASCII("ClawbrowserPrivate");
#else
    return temp_dir_.GetPath().AppendASCII(".config").AppendASCII("clawbrowser-private");
#endif
  }

  ProfileManager CreateProfileManager() const {
    return ProfileManager(GetConfigDir());
  }

  std::string NormalizePathForComparison(const base::FilePath& path) const {
    base::FilePath normalized_path;
    std::string path_string =
        base::NormalizeFilePath(path, &normalized_path)
            ? normalized_path.AsUTF8Unsafe()
            : path.AsUTF8Unsafe();
#if BUILDFLAG(IS_MAC)
    if (path_string.rfind("/var/", 0) == 0 ||
        path_string.rfind("/tmp/", 0) == 0) {
      path_string = "/private" + path_string;
    }
#endif
    return path_string;
  }

  std::string UserDataDirSwitch(const base::CommandLine& cmd) const {
    return NormalizePathForComparison(base::FilePath::FromUTF8Unsafe(
        cmd.GetSwitchValueASCII("user-data-dir")));
  }

  void WriteCachedProfile(const std::string& id) {
    base::FilePath fixture;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &fixture);
    fixture = fixture.AppendASCII("clawbrowser")
                  .AppendASCII("test")
                  .AppendASCII("fixtures")
                  .AppendASCII("valid_fingerprint.json");
    std::string json;
    ASSERT_TRUE(base::ReadFileToString(fixture, &json));
    auto envelope = ProfileEnvelope::Parse(json);
    ASSERT_TRUE(envelope.has_value()) << envelope.error();
    auto save_result = CreateProfileManager().SaveProfile(id, *envelope);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();
  }

  void WriteLegacyRawCachedProfile(const std::string& id) {
    base::FilePath profile_dir =
        GetConfigDir().AppendASCII("Browser").AppendASCII(id);
    ASSERT_TRUE(base::CreateDirectory(profile_dir));

    base::FilePath fixture;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &fixture);
    fixture = fixture.AppendASCII("clawbrowser")
                  .AppendASCII("test")
                  .AppendASCII("fixtures")
                  .AppendASCII("valid_fingerprint.json");
    std::string json;
    ASSERT_TRUE(base::ReadFileToString(fixture, &json));
    ASSERT_TRUE(base::WriteFile(profile_dir.AppendASCII("fingerprint.json"),
                                json));
  }

  void WriteLegacyPrivacyPolicyCachedProfile(const std::string& id) {
    WriteCachedProfile(id);
    ProfileEnvelope envelope = ReadSavedProfile(id);
    envelope.request.runtime_gpu = "apple-metal";
    envelope.response.fingerprint.surface_policy.canvas.mode = "native";
    envelope.response.fingerprint.surface_policy.webgl.mode = "native";
    auto save_result = CreateProfileManager().SaveProfile(id, envelope);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();
  }

  void WriteCachedProfileWithProxy(const std::string& id,
                                   const std::string& username,
                                   const std::string& password) {
    ProfileEnvelope envelope;
    envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
    envelope.created_at = "2026-04-09T12:00:00Z";
    envelope.request.platform = "macos";
    envelope.request.browser = "chrome";
    envelope.request.country = "US";
    envelope.response.fingerprint.user_agent = "stale-ua";
    envelope.response.fingerprint.platform = "MacIntel";
    envelope.response.fingerprint.screen.width = 1920;
    envelope.response.fingerprint.screen.height = 1080;
    envelope.response.fingerprint.screen.avail_width = 1920;
    envelope.response.fingerprint.screen.avail_height = 1040;
    envelope.response.fingerprint.screen.color_depth = 24;
    envelope.response.fingerprint.screen.pixel_ratio = 1.0;
    envelope.response.fingerprint.hardware.concurrency = 8;
    envelope.response.fingerprint.hardware.memory = 8;
    envelope.response.fingerprint.webgl.vendor = "vendor";
    envelope.response.fingerprint.webgl.renderer = "renderer";
    envelope.response.fingerprint.canvas_seed = 1;
    envelope.response.fingerprint.audio_seed = 2;
    envelope.response.fingerprint.client_rects_seed = 3;
    envelope.response.fingerprint.timezone = "UTC";
    envelope.response.fingerprint.language = {"en-US"};
    envelope.response.fingerprint.fonts = {"Arial"};
    envelope.response.proxy.emplace();
    envelope.response.proxy->host = "proxy.example.com";
    envelope.response.proxy->port = 3128;
    envelope.response.proxy->country = "US";
    envelope.response.proxy->city = "New York";
    envelope.response.proxy->username = username;
    envelope.response.proxy->password = password;

    auto save_result = CreateProfileManager().SaveProfile(id, envelope);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();
  }

  void WriteCachedProfileWithRequest(const std::string& id,
                                     const GenerateRequest& request) {
    base::FilePath fixture;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &fixture);
    fixture = fixture.AppendASCII("clawbrowser")
                  .AppendASCII("test")
                  .AppendASCII("fixtures")
                  .AppendASCII("valid_fingerprint.json");
    std::string json;
    ASSERT_TRUE(base::ReadFileToString(fixture, &json));
    auto envelope = ProfileEnvelope::Parse(json);
    ASSERT_TRUE(envelope.has_value()) << envelope.error();
    envelope->request = request;
    auto save_result = CreateProfileManager().SaveProfile(id, *envelope);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();
  }

  ProfileEnvelope ReadSavedProfile(const std::string& id) {
    auto read_result = CreateProfileManager().ReadProfile(id);
    EXPECT_TRUE(read_result.has_value()) << read_result.error();
    return *read_result;
  }

  void AddGenerateErrorResponse(const std::string& body,
                                net::HttpStatusCode status) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1),
                                          HttpStatusLine(status))
            .Build();
    url_loader_factory_.AddResponse(
        GURL(std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate"),
        std::move(head), body, network::URLLoaderCompletionStatus(net::OK));
  }

  std::string GenerateSuccessResponseJson(
      const std::string& user_agent,
      const std::string& proxy_json = "") const {
    std::string proxy_field;
    if (!proxy_json.empty()) {
      proxy_field = ",\"proxy\":" + proxy_json;
    }
    return base::StringPrintf(R"json({
      "fingerprint": {
        "browser_family": "chrome",
        "browser_version": "120.0.0.0",
        "engine": "blink",
        "os": "macos",
        "os_version": "10.15.7",
        "architecture": "arm64",
        "device_class": "desktop",
        "user_agent": "%s",
        "platform": "MacIntel",
        "user_agent_data": {
          "brands": [
            {"brand": "Chromium", "version": "120"},
            {"brand": "Google Chrome", "version": "120"},
            {"brand": "Not/A)Brand", "version": "99"}
          ],
          "fullVersionList": [
            {"brand": "Chromium", "version": "120.0.0.0"},
            {"brand": "Google Chrome", "version": "120.0.0.0"},
            {"brand": "Not/A)Brand", "version": "99.0.0.0"}
          ],
          "platform": "macOS",
          "platformVersion": "10.15.7",
          "architecture": "arm",
          "bitness": "64",
          "mobile": false,
          "model": "",
          "uaFullVersion": "120.0.0.0"
        },
        "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                   "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
        "hardware": {"concurrency": 8, "memory": 8},
        "webgl": {
          "vendor": "Google Inc. (Google)",
          "renderer": "ANGLE (Google, Vulkan 1.3.0 (SwiftShader Device), SwiftShader driver)"
        },
        "canvas_seed": 1, "audio_seed": 2, "client_rects_seed": 3,
        "timezone": "UTC",
        "language": ["en"],
        "fonts": ["Arial"],
        "headers": {
          "User-Agent": "%s",
          "Accept-Language": "en"
        },
        "surface_policy": {
          "canvas": {"mode": "override"},
          "audio": {"mode": "native"},
          "client_rects": {"mode": "native"},
          "webgl": {"mode": "native"},
          "fonts": {"mode": "native_or_allowlist"},
          "plugins": {"mode": "override"},
          "media_devices": {"mode": "override"},
          "speech_voices": {"mode": "override"}
        }
      }
      %s
    })json",
                              user_agent.c_str(), user_agent.c_str(),
                              proxy_field.c_str());
  }

  void AddGenerateSuccessResponseWithProxy(const std::string& user_agent,
                                           const std::string& username,
                                           const std::string& password) {
    std::string proxy_json = base::StringPrintf(R"json({
      "host": "proxy.example.com",
      "port": 3128,
      "country": "US",
      "city": "New York",
      "username": "%s",
      "password": "%s"
    })json",
                                                username.c_str(),
                                                password.c_str());
    url_loader_factory_.AddResponse(
        std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
        GenerateSuccessResponseJson(user_agent, proxy_json));
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::ScopedPathOverride> local_app_data_override_;
#else
  std::unique_ptr<base::ScopedPathOverride> home_override_;
#endif
  std::unique_ptr<base::Environment> env_;
  // Records the body of every fingerprint generate request.
  void CaptureGenerateRequestBodies() {
    url_loader_factory_.SetInterceptor(base::BindRepeating(
        [](std::vector<std::string>* bodies,
           const network::ResourceRequest& request) {
          if (request.url.path() == "/v1/fingerprints/generate") {
            bodies->push_back(network::GetUploadData(request));
          }
        },
        &generate_request_bodies_));
  }

  base::DictValue LastGenerateRequestBody() {
    EXPECT_FALSE(generate_request_bodies_.empty());
    if (generate_request_bodies_.empty()) {
      return base::DictValue();
    }
    std::optional<base::Value> parsed =
        base::JSONReader::Read(generate_request_bodies_.back(),
                               base::JSON_PARSE_RFC);
    EXPECT_TRUE(parsed.has_value() && parsed->is_dict());
    return parsed.has_value() && parsed->is_dict()
               ? std::move(*parsed).TakeDict()
               : base::DictValue();
  }

  std::vector<std::string> generate_request_bodies_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(StartupTest, ListProfiles) {
  WriteCachedProfile("list_profile");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("list");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->should_exit);
  EXPECT_EQ(result->exit_code, 0);
}

TEST_F(StartupTest, HandleBasicStartupCompleteListsProfiles) {
  WriteCachedProfile("list_profile");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("list");
  cmd.AppendSwitchASCII("output", "json");

  testing::internal::CaptureStdout();
  auto result = HandleBasicStartupComplete(cmd);
  std::string stdout_output = testing::internal::GetCapturedStdout();

  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(result->value(), 0);
  EXPECT_NE(stdout_output.find("\"id\": \"list_profile\""), std::string::npos);
}

#if BUILDFLAG(IS_MAC)
TEST_F(StartupTest, MissingMacCatalogStopsManagedStartup) {
  WriteCachedProfile("missing_catalog");
  WriteConfigJson("test_key");
  base::apple::SetOverrideFrameworkBundlePath(temp_dir_.GetPath());
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "missing_catalog");
  cmd.AppendArg("https://example.com/");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, CorruptMacCatalogStopsManagedStartup) {
  WriteCachedProfile("corrupt_catalog");
  WriteConfigJson("test_key");
  const auto resources = temp_dir_.GetPath().AppendASCII("Resources");
  ASSERT_TRUE(base::CopyDirectory(
      CatalogBundle().GetPath().AppendASCII("Resources"), resources, true));
  ASSERT_TRUE(base::WriteFile(resources.AppendASCII("clawbrowser-fonts")
      .AppendASCII(kLinuxFontCatalogID).AppendASCII("manifest.json"), "{}"));
  base::apple::SetOverrideFrameworkBundlePath(temp_dir_.GetPath());
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "corrupt_catalog");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}
#endif

TEST_F(StartupTest, ExperimentalNormalizedCanvasPreservesManagedGuards) {
  WriteCachedProfile("normalized_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "normalized_profile");
  cmd.AppendSwitch(kExperimentalNormalizedCanvasSwitch);
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(FingerprintAccessor::Get()->canvas_spoofing_enabled);
  EXPECT_EQ(FingerprintAccessor::Get()->experimental_normalized_canvas,
            BUILDFLAG(IS_LINUX));
  EXPECT_TRUE(cmd.HasSwitch(kRequireFingerprintSwitch));
  EXPECT_EQ(cmd.GetSwitchValueASCII("proxy-server"), "http://proxy.example.com:8080");
  EXPECT_EQ(cmd.GetSwitchValueASCII("force-webrtc-ip-handling-policy"),
            "disable_non_proxied_udp");
}

TEST_F(StartupTest, FingerprintWithCachedProfile) {
  WriteCachedProfile("cached_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  // Fingerprint should be loaded
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(FingerprintAccessor::Get()->canvas_spoofing_enabled);
  EXPECT_FALSE(FingerprintAccessor::Get()->webgl_spoofing_enabled);
  EXPECT_FALSE(FingerprintAccessor::Get()->experimental_normalized_canvas);
  // Command line flags should be set
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_TRUE(cmd.HasSwitch(kRequireFingerprintSwitch));
  EXPECT_TRUE(cmd.HasSwitch(kDisableWebGLSpoofingSwitch));
  EXPECT_FALSE(cmd.HasSwitch("clawbrowser-fp-data"));
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_TRUE(cmd.HasSwitch("proxy-server"));
  EXPECT_EQ(cmd.GetSwitchValueASCII("proxy-server"),
            "http://proxy.example.com:8080");
  EXPECT_EQ(cmd.GetSwitchValueASCII("webrtc-ip-handling-policy"),
            "disable_non_proxied_udp");
  EXPECT_EQ(cmd.GetSwitchValueASCII("force-webrtc-ip-handling-policy"),
            "disable_non_proxied_udp");
  EXPECT_TRUE(cmd.HasSwitch("lang"));
  EXPECT_TRUE(cmd.HasSwitch("accept-lang"));
  EXPECT_TRUE(cmd.HasSwitch("user-agent"));
  EXPECT_EQ(cmd.GetSwitchValueASCII("user-agent"),
            FingerprintAccessor::Get()->user_agent);
  // User-agent client hints are no longer plumbed through command-line
  // switches: a5fc4ed replaced clawbrowser-ua-{major-version,full-version,
  // platform} with the ChromeContentBrowserClient::GetUserAgentMetadata()
  // override (patch 024) that returns BuildUserAgentMetadata(fingerprint).
  // That path is covered by clawbrowser/test/integration/test_surfaces.py
  // (test_navigator_user_agent_data, test_sec_ch_ua_headers).
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintChildDataSwitch));
#else
  EXPECT_FALSE(cmd.HasSwitch(kFingerprintChildDataSwitch));
#endif
}

TEST(SurfaceSpoofingResolutionTest, PolicyEnablesWithoutAnyFlag) {
  // The regression this guards: spoofing used to require --enable-*-spoofing
  // even when the profile asked for "override", so a fingerprint could carry a
  // WebGL renderer string and still emit the host's real adapter.
  EXPECT_TRUE(ResolveSurfaceSpoofing(/*forced_on=*/false, /*forced_off=*/false,
                                     /*policy_requests_override=*/true));
}

TEST(SurfaceSpoofingResolutionTest, NativePolicyStaysOff) {
  EXPECT_FALSE(ResolveSurfaceSpoofing(false, false, false));
}

TEST(SurfaceSpoofingResolutionTest, EnableSwitchForcesOnAgainstNativePolicy) {
  EXPECT_TRUE(ResolveSurfaceSpoofing(/*forced_on=*/true, /*forced_off=*/false,
                                     /*policy_requests_override=*/false));
}

TEST(SurfaceSpoofingResolutionTest, DisableSwitchBeatsEverything) {
  EXPECT_FALSE(ResolveSurfaceSpoofing(/*forced_on=*/true, /*forced_off=*/true,
                                      /*policy_requests_override=*/true));
  EXPECT_FALSE(ResolveSurfaceSpoofing(false, true, true));
}

TEST_F(StartupTest, WindowSizeFitsInsideSpoofedScreen) {
  // window.outerWidth/outerHeight come from the real OS window and are not
  // spoofed, so a window larger than the screen we advertise is a one-line
  // contradiction for any detector. The fixture reports 1920x1080 with a
  // 1920x1040 available area.
  WriteCachedProfile("cached_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);

  ASSERT_TRUE(cmd.HasSwitch("window-size"));
  const std::string value = cmd.GetSwitchValueASCII("window-size");
  const size_t comma = value.find(',');
  ASSERT_NE(comma, std::string::npos) << "window-size was: " << value;

  int width = 0;
  int height = 0;
  ASSERT_TRUE(base::StringToInt(value.substr(0, comma), &width));
  ASSERT_TRUE(base::StringToInt(value.substr(comma + 1), &height));

  const auto* fp = FingerprintAccessor::Get();
  EXPECT_GT(width, 0);
  EXPECT_GT(height, 0);
  EXPECT_LE(width, fp->screen.avail_width);
  EXPECT_LE(height, fp->screen.avail_height);
  EXPECT_LE(fp->screen.avail_width, fp->screen.width);
  EXPECT_LE(fp->screen.avail_height, fp->screen.height);

  // Anchored at the origin so screenX + outerWidth stays on-screen too.
  EXPECT_EQ(cmd.GetSwitchValueASCII("window-position"), "0,0");
}

TEST_F(StartupTest, ExplicitWindowSizeIsNotOverridden) {
  // A caller-supplied size is a deliberate, controlled override and must win,
  // while an unspecified position is still anchored at the virtual origin.
  WriteCachedProfile("cached_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");
  cmd.AppendSwitchASCII("window-size", "801,601");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(cmd.GetSwitchValueASCII("window-size"), "801,601");
  EXPECT_EQ(cmd.GetSwitchValueASCII("window-position"), "0,0");
}

TEST_F(StartupTest, ExplicitWindowPositionIsNotOverridden) {
  WriteCachedProfile("cached_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");
  cmd.AppendSwitchASCII("window-position", "317,223");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(cmd.GetSwitchValueASCII("window-position"), "317,223");
}

TEST_F(StartupTest, SwiftShaderDisablesWebGLStringSpoofingEvenWhenForced) {
  WriteCachedProfile("cached_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");
  cmd.AppendSwitch(kEnableCanvasSpoofingSwitch);
  cmd.AppendSwitch(kEnableWebGLSpoofingSwitch);

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());

  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(FingerprintAccessor::Get()->canvas_spoofing_enabled);
  EXPECT_FALSE(FingerprintAccessor::Get()->webgl_spoofing_enabled);
  EXPECT_TRUE(cmd.HasSwitch(kEnableWebGLSpoofingSwitch));
  EXPECT_TRUE(cmd.HasSwitch(kDisableWebGLSpoofingSwitch));
}

TEST_F(StartupTest, FingerprintWithCachedProfileWithoutPrefix) {
  WriteLegacyRawCachedProfile("custom_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "custom_profile");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("custom_profile")));
}

TEST_F(StartupTest, FingerprintWithCachedProfilePathLikeId) {
  WriteCachedProfile("group/profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "group/profile");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("group/profile")));
}

TEST_F(StartupTest, FingerprintWithCachedProfileEmptyId) {
  WriteCachedProfile("");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(CreateProfileManager().GetUserDataDir("")));
}

TEST_F(StartupTest, ConfigureEarlyStartupSetsFingerprintUserDataDir) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "early_profile");

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("early_profile")));
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ConfigureEarlyStartupIsolatesFingerprintWebGL) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "isolated_webgl_profile");
  cmd.AppendSwitchASCII("use-gl", "desktop");
  cmd.AppendSwitchASCII("use-angle", "metal");
  cmd.AppendSwitchASCII("use-webgpu-adapter", "default");
  cmd.AppendSwitch(kDisableWebGLSpoofingSwitch);

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(cmd.GetSwitchValueASCII("use-gl"), "angle");
  EXPECT_EQ(cmd.GetSwitchValueASCII("use-angle"), "swiftshader");
  EXPECT_EQ(cmd.GetSwitchValueASCII("use-webgpu-adapter"), "swiftshader");
  EXPECT_TRUE(cmd.HasSwitch(kDisableWebGLSpoofingSwitch));
}

TEST_F(StartupTest, ConfigureEarlyStartupPinsLinuxAndWindowsCanvasRasterizer) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "isolated_canvas_profile");
  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  EXPECT_TRUE(cmd.HasSwitch("disable-accelerated-2d-canvas"));
#else
  EXPECT_FALSE(cmd.HasSwitch("disable-accelerated-2d-canvas"));
#endif
}

TEST_F(StartupTest, ConfigureEarlyStartupKeepsVanillaCanvasRasterizer) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(cmd.HasSwitch("disable-accelerated-2d-canvas"));
}

TEST_F(StartupTest, ConfigureEarlyStartupKeepsDisableGPUWithSwiftShader) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "software_webgl_profile");
  cmd.AppendSwitch("disable-gpu");

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_TRUE(cmd.HasSwitch("disable-gpu"));
  EXPECT_EQ(cmd.GetSwitchValueASCII("use-gl"), "angle");
  EXPECT_EQ(cmd.GetSwitchValueASCII("use-angle"), "swiftshader");
  EXPECT_EQ(cmd.GetSwitchValueASCII("use-webgpu-adapter"), "swiftshader");
  EXPECT_TRUE(cmd.HasSwitch(kDisableWebGLSpoofingSwitch));
}

TEST_F(StartupTest, CommandLineBeforeUserDataDirRecordsManagedFingerprintDir) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "managed_profile");

  ConfigureCommandLineBeforeUserDataDir(&cmd);

  EXPECT_EQ(NormalizePathForComparison(GetManagedFingerprintUserDataDir()),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("managed_profile")));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(GetManagedFingerprintUserDataDir()));
}

TEST_F(StartupTest, ConfigureEarlyStartupEnablesMockKeychainOnMac) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(cmd.HasSwitch("use-mock-keychain"));
#else
  EXPECT_FALSE(cmd.HasSwitch("use-mock-keychain"));
#endif
}

TEST_F(StartupTest, ConfigureEarlyStartupSetsPathLikeFingerprintUserDataDir) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "group/profile");

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("group/profile")));
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ConfigureEarlyStartupSetsEmptyFingerprintUserDataDir) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "");

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(CreateProfileManager().GetUserDataDir("")));
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, RequiredProxyRejectsBrowserNativeProxyOverrides) {
  WriteConfigJson("test_key");
  constexpr const char* kConflictingSwitches[] = {
      "proxy-server",
      "no-proxy-server",
      "proxy-pac-url",
      "proxy-auto-detect",
      "proxy-bypass-list",
      "webrtc-ip-handling-policy",
      "force-webrtc-ip-handling-policy",
  };

  for (const char* switch_name : kConflictingSwitches) {
    SCOPED_TRACE(switch_name);
    base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
    cmd.AppendSwitchASCII("fingerprint", "required_proxy_conflict");
    cmd.AppendSwitch(kRequireProxySwitch);
    cmd.AppendSwitchASCII(switch_name, "caller-value");

    auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());

    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_TRUE(result->should_exit);
    EXPECT_EQ(result->exit_code, 1);
    EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
  }
}

TEST_F(StartupTest, VerboseLogging) {
  WriteCachedProfile("verbose_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "verbose_profile");
  cmd.AppendSwitch("verbose");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(IsVerbose());
}

TEST_F(StartupTest, MultipleFingerprintFlagsLastWins) {
  // When --fingerprint is specified multiple times, last value wins
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "first_profile");
  cmd.AppendSwitchASCII("fingerprint", "second_profile");

  ClawArgs args = ClawArgs::Parse(cmd);
  // base::CommandLine last-wins semantics for duplicate switches
  EXPECT_EQ(args.fingerprint_id(), "second_profile");
}

TEST_F(StartupTest, VanillaStartsWithoutVendorCredentials) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendArg("https://example.test/");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->should_exit);
  ASSERT_EQ(cmd.GetArgs().size(), 1u);
  EXPECT_EQ(cmd.GetArgs()[0], FILE_PATH_LITERAL("https://example.test/"));
  EXPECT_EQ(cmd.GetSwitchValuePath("user-data-dir"),
            CreateProfileManager().GetVanillaUserDataDir());
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(StartupTest, LocalCachedProfileStartsWithoutVendorCredentials) {
  WriteCachedProfile("local");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "local");
  cmd.AppendSwitch("skip-verify");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->should_exit);
  EXPECT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(StartupTest, MissingLocalProfileFailsWithoutNetwork) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "missing");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->should_exit);
  EXPECT_NE(result->exit_code, 0);
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(StartupTest, RegenerationPreservesLocalProfileWithoutNetwork) {
  WriteCachedProfile("local");
  auto path = CreateProfileManager().GetFingerprintPath("local");
  std::string before;
  ASSERT_TRUE(base::ReadFileToString(path, &before));
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "local");
  cmd.AppendSwitch("regenerate");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->should_exit);
  EXPECT_NE(result->exit_code, 0);
  std::string after;
  ASSERT_TRUE(base::ReadFileToString(path, &after));
  EXPECT_EQ(before, after);
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
}

TEST_F(StartupTest, EarlyStartupUsesLocalProfileWithoutCredentials) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "local");
  ConfigureCommandLineBeforeUserDataDir(&cmd);
  EXPECT_EQ(cmd.GetSwitchValuePath("user-data-dir"),
            CreateProfileManager().GetUserDataDir("local"));
  EXPECT_TRUE(cmd.GetArgs().empty());
}

TEST_F(StartupTest, EarlyStartupDefaultsToVanillaLocally) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  ConfigureCommandLineBeforeUserDataDir(&cmd);
  EXPECT_FALSE(cmd.HasSwitch("fingerprint"));
  EXPECT_EQ(cmd.GetSwitchValuePath("user-data-dir"),
            CreateProfileManager().GetVanillaUserDataDir());
}

}  // namespace
}  // namespace clawbrowser
