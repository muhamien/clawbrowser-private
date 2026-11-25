#include "clawbrowser/startup.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/path_service.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/defaults.h"
#include "clawbrowser/cli/profile_manager.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/fingerprint_coherence.h"
#include "clawbrowser/fingerprint_loader.h"
#include "clawbrowser/font_catalog.h"
#include "clawbrowser/font_catalog_identity.h"
#include "clawbrowser/logging.h"
#include "clawbrowser/paths.h"
#include "clawbrowser/proxy/proxy_config.h"
#include "clawbrowser/proxy/socks5_auth_proxy_bridge.h"
#if BUILDFLAG(IS_WIN)
#include "clawbrowser/windows_installed_fonts.h"
#endif
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "crypto/hash.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "font_catalog_build.h"
#endif

namespace clawbrowser {

namespace {

base::FilePath& ManagedFingerprintUserDataDir() {
  static base::NoDestructor<base::FilePath> dir;
  return *dir;
}

void PrintError(const ClawArgs& args, const std::string& code,
                const std::string& message) {
  // Errors always print to stderr regardless of --verbose
  fprintf(stderr, "[clawbrowser] error: %s\n", message.c_str());
  if (args.json_output()) {
    base::DictValue error;
    error.Set("error", code);
    error.Set("message", message);
    std::string json;
    base::JSONWriter::Write(base::Value(std::move(error)), &json);
    fprintf(stdout, "%s\n", json.c_str());
  }
}

StartupResult HandleListProfiles(const ClawArgs& args,
                                 ProfileManager* profile_manager) {
  auto profiles = profile_manager->ListProfiles();
  if (args.json_output()) {
    base::ListValue list;
    for (const auto& p : profiles) {
      base::DictValue item;
      item.Set("id", p.id);
      item.Set("created_at", p.created_at);
      item.Set("country", p.country);
      list.Append(std::move(item));
    }
    std::string json;
    base::JSONWriter::WriteWithOptions(
        base::Value(std::move(list)),
        base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
    fprintf(stdout, "%s\n", json.c_str());
  } else {
    for (const auto& p : profiles) {
      fprintf(stdout, "%s  %s  %s\n",
              p.id.c_str(), p.created_at.c_str(), p.country.c_str());
    }
  }

  StartupResult result;
  result.should_exit = true;
  return result;
}

void ApplyMacAutomationSwitches(base::CommandLine* command_line) {
#if BUILDFLAG(IS_MAC)
  if (!command_line->HasSwitch("use-mock-keychain")) {
    command_line->AppendSwitch("use-mock-keychain");
  }
#endif
}

void ConfigureVanillaUserDataDir(base::CommandLine* command_line,
                                 ProfileManager* profile_manager);

bool IsSwiftShaderWebGLBackend(const base::CommandLine& command_line) {
  const std::string use_gl = command_line.GetSwitchValueASCII("use-gl");
  const std::string use_angle = command_line.GetSwitchValueASCII("use-angle");
  return base::StartsWith(use_gl, "swiftshader",
                          base::CompareCase::INSENSITIVE_ASCII) ||
         base::StartsWith(use_angle, "swiftshader",
                          base::CompareCase::INSENSITIVE_ASCII);
}

// A renderer string can only be spoofed coherently when all of the WebGL
// capabilities behind it belong to the same adapter. Fingerprint profiles
// therefore use ANGLE's bundled SwiftShader backend: it hides the host GPU at
// the source and gives every platform a real, internally consistent software
// adapter. Vanilla mode remains available when native GPU acceleration is
// preferred over fingerprint isolation.
void ApplyFingerprintWebGLIsolation(const ClawArgs& args,
                                    base::CommandLine* command_line) {
  if (args.list() || args.is_vanilla()) {
    return;
  }

  command_line->RemoveSwitch("use-gl");
  command_line->RemoveSwitch("use-angle");
  command_line->RemoveSwitch("use-webgpu-adapter");
  command_line->AppendSwitchASCII("use-gl", "angle");
  command_line->AppendSwitchASCII("use-angle", "swiftshader");
  // WebGPU selects a Dawn adapter independently from ANGLE. Pin it to the
  // bundled fallback adapter as well; otherwise WebGL reports SwiftShader
  // while navigator.gpu still exposes the host Metal/D3D/Vulkan device.
  command_line->AppendSwitchASCII("use-webgpu-adapter", "swiftshader");
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  // Canvas2D otherwise selects CPU or GPU rasterization independently of the
  // WebGL adapter. Those paths produce different observable pixels even with
  // identical bundled fonts: on Windows a default 2D context and one created
  // with willReadFrequently hash differently for the same drawing. Keep one
  // raster path for Linux and Windows identities; vanilla mode above retains
  // the user's acceleration preference.
  command_line->AppendSwitch("disable-accelerated-2d-canvas");
#endif

  // Renderer/GPU children reconstruct the surface policy from the profile and
  // raw command-line switches. Force the resolved native WebGL policy onto
  // their command lines too, including while an older backend or cached
  // profile still says "override". Otherwise those children would put the
  // stale vendor/renderer overlay back on top of SwiftShader's real limits.
  if (!command_line->HasSwitch(kDisableWebGLSpoofingSwitch)) {
    command_line->AppendSwitch(kDisableWebGLSpoofingSwitch);
  }
}

void ConfigureProfileStartupCommandLine(const ClawArgs& args,
                                        base::CommandLine* command_line,
                                        ProfileManager* profile_manager) {
  if (args.list())
    return;
  if (args.is_vanilla()) {
    ConfigureVanillaUserDataDir(command_line, profile_manager);
    return;
  }
  ApplyFingerprintWebGLIsolation(args, command_line);
  ManagedFingerprintUserDataDir() =
      profile_manager->GetUserDataDir(args.fingerprint_id());
  command_line->AppendSwitchPath("user-data-dir",
                                 ManagedFingerprintUserDataDir());
}

void ConfigureVanillaUserDataDir(base::CommandLine* command_line,
                                 ProfileManager* profile_manager) {
  command_line->AppendSwitchPath(
      "user-data-dir", profile_manager->GetVanillaUserDataDir());
}

std::unique_ptr<Socks5AuthProxyBridge>& ActiveSocks5AuthProxyBridge() {
  static base::NoDestructor<std::unique_ptr<Socks5AuthProxyBridge>> bridge;
  return *bridge;
}

void StopSocks5AuthProxyBridge() {
  ActiveSocks5AuthProxyBridge().reset();
}

StartupResult FailManagedFingerprintStartup(const ClawArgs& args,
                                            const std::string& code,
                                            const std::string& message) {
  StopSocks5AuthProxyBridge();
  FingerprintAccessor::Reset();
  PrintError(args, code, message);

  StartupResult result;
  result.should_exit = true;
  result.exit_code = 1;
  return result;
}

std::optional<std::string> ConflictingRequiredProxySwitch(
    const base::CommandLine& command_line) {
  // A require-proxy launch gets its complete proxy and WebRTC policy from the
  // validated fingerprint response.  Reject browser-native alternatives at
  // the process boundary: --no-proxy-server and PAC/auto-detect take
  // precedence over --proxy-server, while a bypass list can silently route
  // selected destinations directly.  Reject policy overrides as well instead
  // of depending on duplicate-switch ordering.
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
    if (command_line.HasSwitch(switch_name)) {
      return std::string(switch_name);
    }
  }
  return std::nullopt;
}

std::string HeaderOrValue(const RuntimeFingerprint& fp,
                          const std::string& header,
                          const std::string& fallback) {
  for (const auto& [name, value] : fp.headers) {
    if (base::EqualsCaseInsensitiveASCII(name, header) && !value.empty()) {
      return value;
    }
  }
  return fallback;
}

// Chromium's --accept-lang switch feeds the intl.accept_languages pref, which
// net::HttpUtil::ExpandLanguageList() parses as bare language codes; it CHECKs
// that no entry contains ';' or whitespace. Backend fingerprints supply a full
// Accept-Language header value, which normally carries q-values such as
// "en-US,en;q=0.9". Strip the parameters so only the codes are handed over --
// Chromium regenerates the q-values itself when building the actual header.
std::string StripLanguageQualityValues(std::string_view header_value) {
  std::vector<std::string> codes;
  for (std::string_view entry : base::SplitStringPiece(
           header_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::string_view code = entry.substr(0, entry.find(';'));
    code = base::TrimWhitespaceASCII(code, base::TRIM_ALL);
    if (!code.empty())
      codes.emplace_back(code);
  }
  return base::JoinString(codes, ",");
}

std::string AcceptLanguageFromFingerprint(const RuntimeFingerprint& fp) {
  for (const auto& [name, value] : fp.headers) {
    if (base::EqualsCaseInsensitiveASCII(name, "Accept-Language") &&
        !value.empty()) {
      return StripLanguageQualityValues(value);
    }
  }

  std::string accept_lang;
  for (size_t i = 0; i < fp.language.size(); ++i) {
    if (i > 0)
      accept_lang += ",";
    accept_lang += fp.language[i];
  }
  return accept_lang;
}

base::expected<std::optional<RuntimeProxyConfig>, std::string>
ResolveDevProxyOverride() {
#if defined(CLAWBROWSER_ENABLE_DEV_PROXY_URL_OVERRIDE) && \
    CLAWBROWSER_ENABLE_DEV_PROXY_URL_OVERRIDE
  auto env = base::Environment::Create();
  std::optional<std::string> proxy_url =
      env->GetVar("CLAWBROWSER_DEV_PROXY_URL");
  if (!proxy_url.has_value() || proxy_url->empty()) {
    return base::ok(std::nullopt);
  }

  auto parsed = ParseProxyUrl(*proxy_url);
  if (!parsed.has_value()) {
    return base::unexpected(parsed.error());
  }
  if (parsed->scheme.value_or("http") != "socks5") {
    return base::unexpected(
        "CLAWBROWSER_DEV_PROXY_URL only supports socks5:// URLs");
  }
  return base::ok(std::optional<RuntimeProxyConfig>(std::move(*parsed)));
#else
  return base::ok(std::nullopt);
#endif
}

void ReplaceRuntimeProxy(RuntimeProxyConfig proxy) {
  const RuntimeFingerprint* fp = FingerprintAccessor::Get();
  if (!fp) {
    return;
  }
  FingerprintAccessor::Set(*fp, std::move(proxy));
}

base::expected<std::optional<RuntimeProxyConfig>, std::string>
ApplyDevProxyOverride() {
  auto proxy_override = ResolveDevProxyOverride();
  if (!proxy_override.has_value()) {
    return base::unexpected(proxy_override.error());
  }
  if (!proxy_override->has_value()) {
    return base::ok(std::nullopt);
  }
  RuntimeProxyConfig proxy = std::move(proxy_override->value());
  ReplaceRuntimeProxy(proxy);
  return base::ok(std::optional<RuntimeProxyConfig>(std::move(proxy)));
}

base::expected<void, std::string> ConfigureBundledFontsBeforeThreads() {
#if BUILDFLAG(IS_WIN)
  base::FilePath module_dir;
  if (!base::PathService::Get(base::DIR_MODULE, &module_dir))
    return base::unexpected("cannot locate managed font resource module");
  auto valid = ValidateFontCatalog(module_dir.AppendASCII(kFontCatalogDirectory),
                                   kFontCatalogManifestHash);
  if (!valid.has_value()) return base::unexpected(valid.error());
#endif
#if BUILDFLAG(IS_MAC)
  auto valid = ValidateFontCatalog(
      base::apple::FrameworkBundlePath().AppendASCII("Resources").AppendASCII(kFontCatalogDirectory),
      kFontCatalogManifestHash);
  if (!valid.has_value()) return base::unexpected(valid.error());
#endif
#if BUILDFLAG(IS_LINUX)
  base::FilePath executable_dir;
  if (!base::PathService::Get(base::DIR_EXE, &executable_dir))
    return base::unexpected("cannot locate bundled font catalog");
  const auto catalog_dir = executable_dir.AppendASCII(kFontCatalogDirectory);
  const auto config_dir = GetClawbrowserConfigDir().AppendASCII("fontconfig");
  const auto cache_dir = config_dir.AppendASCII("cache");
  auto xml = BuildFontCatalogConfig(catalog_dir, cache_dir,
                                    kFontCatalogManifestHash);
  if (!xml.has_value())
    return base::unexpected(xml.error());
  if (!base::CreateDirectory(config_dir))
    return base::unexpected("cannot create managed font configuration directory");
  // Content-address the file: simultaneous launches from different installs
  // must not overwrite one another's absolute asset paths.
  const auto digest = base::ToLowerASCII(
      base::HexEncode(crypto::hash::Sha256(std::string_view(*xml))));
  const auto config_path = config_dir.AppendASCII(digest + ".conf");
  std::string existing;
  if (!base::ReadFileToString(config_path, &existing) || existing != *xml) {
    if (!base::ImportantFileWriter::WriteFileAtomically(config_path, *xml))
      return base::unexpected("cannot write managed font configuration");
  }
  auto environment = base::Environment::Create();
  if (!environment->UnSetVar("FONTCONFIG_SYSROOT") ||
      !environment->UnSetVar("FONTCONFIG_PATH") ||
      !environment->SetVar("FONTCONFIG_FILE", config_path.AsUTF8Unsafe())) {
    return base::unexpected("cannot select managed font configuration");
  }
#endif
  return base::ok();
}

base::expected<std::optional<ProxyBridgeEndpoint>, std::string>
PrepareProxyBridge(const RuntimeProxyConfig& proxy) {
  StopSocks5AuthProxyBridge();
  if (!ShouldUseSocks5AuthBridge(proxy)) {
    return base::ok(std::nullopt);
  }

  auto bridge = Socks5AuthProxyBridge::Start(proxy);
  if (!bridge.has_value()) {
    return base::unexpected(bridge.error());
  }

  ProxyBridgeEndpoint endpoint = (*bridge)->endpoint();
  ActiveSocks5AuthProxyBridge() = std::move(*bridge);
  return base::ok(std::optional<ProxyBridgeEndpoint>(std::move(endpoint)));
}

}  // namespace

base::expected<std::optional<int>, std::string> HandleBasicStartupComplete(
    const base::CommandLine& command_line) {
  ClawArgs args = ClawArgs::Parse(command_line);
  SetVerbose(args.verbose());

  if (!args.list()) {
    // BasicStartupComplete runs before PreSandboxStartup/font initialization.
    // Children inherit the browser's closed catalog; do not read profile auth
    // or rewrite the font environment inside a sandboxed child process.
    if (command_line.GetSwitchValueASCII("type").empty() &&
        !command_line.HasSwitch("version") &&
        !command_line.HasSwitch("help")) {
      if (args.has_fingerprint()) {
        auto fonts = ConfigureBundledFontsBeforeThreads();
        if (!fonts.has_value())
          return base::unexpected(fonts.error());
      }
    }
    return base::ok(std::nullopt);
  }

  ProfileManager profile_manager(GetClawbrowserConfigDir());
  HandleListProfiles(args, &profile_manager);
  return base::ok(0);
}

void ConfigureCommandLineBeforeUserDataDir(base::CommandLine* command_line) {
  ManagedFingerprintUserDataDir().clear();
  ApplyMacAutomationSwitches(command_line);

  ProfileManager profile_manager(GetClawbrowserConfigDir());

  ClawArgs args = ClawArgs::Parse(*command_line);
  SetVerbose(args.verbose());
  ConfigureProfileStartupCommandLine(args, command_line, &profile_manager);
}

const base::FilePath& GetManagedFingerprintUserDataDir() {
  return ManagedFingerprintUserDataDir();
}

void PrintStartupError(const std::string& message) {
  fprintf(stderr, "[clawbrowser] error: %s\n", message.c_str());
}

base::expected<StartupResult, std::string> ConfigureEarlyStartup(
    base::CommandLine* command_line) {
  ConfigureCommandLineBeforeUserDataDir(command_line);
  auto basic_startup_result = HandleBasicStartupComplete(*command_line);
  if (!basic_startup_result.has_value()) {
    return base::unexpected(basic_startup_result.error());
  }

  StartupResult result;
  if (basic_startup_result->has_value()) {
    result.should_exit = true;
    result.exit_code = basic_startup_result->value();
    return base::ok(std::move(result));
  }

  return base::ok(std::move(result));
}

base::expected<StartupResult, std::string> RunStartup(
    base::CommandLine* command_line,
    scoped_refptr<network::SharedURLLoaderFactory> /*url_loader_factory*/) {
  ApplyMacAutomationSwitches(command_line);
  ProfileManager profile_manager(GetClawbrowserConfigDir());
  StartupResult result;

  auto basic_startup_result = HandleBasicStartupComplete(*command_line);
  if (!basic_startup_result.has_value()) {
    return base::unexpected(basic_startup_result.error());
  }
  if (basic_startup_result->has_value()) {
    result.should_exit = true;
    result.exit_code = basic_startup_result->value();
    return base::ok(std::move(result));
  }

  ClawArgs args = ClawArgs::Parse(*command_line);
  SetVerbose(args.verbose());

  if (args.require_proxy()) {
    if (std::optional<std::string> conflict =
            ConflictingRequiredProxySwitch(*command_line);
        conflict.has_value()) {
      return base::ok(FailManagedFingerprintStartup(
          args, "conflicting_proxy_switch",
          "profile launch requires its validated proxy, but --" + *conflict +
              " was also supplied"));
    }
  }

  CLAW_VLOG() << "starting with args: fingerprint="
              << args.fingerprint_id();

  if (args.regenerate() || args.has_location_overrides() ||
      args.has_proxy_scheme_override()) {
    return base::ok(FailManagedFingerprintStartup(
        args, "local_profile_update_required",
        "edit and validate the local profile before changing its identity"));
  }

  if (args.is_vanilla()) {
    if (args.require_proxy()) {
      return base::ok(FailManagedFingerprintStartup(
          args, "local_proxy_profile_required",
          "select a local fingerprint profile containing a proxy"));
    }
    StopSocks5AuthProxyBridge();
    ConfigureVanillaUserDataDir(command_line, &profile_manager);
    return base::ok(std::move(result));
  }

  const std::string& fp_id = args.fingerprint_id();
  auto local_profile = profile_manager.ReadProfile(fp_id);
  if (!local_profile.has_value()) {
    return base::ok(FailManagedFingerprintStartup(
        args, "local_profile_unavailable", local_profile.error()));
  }
  ApplyFingerprintWebGLIsolation(args, command_line);

  // The installed runtime decides which fonts exist: the bundled catalog, or
  // the default Windows families for a Windows fingerprint on a Windows host.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  auto font_profile = profile_manager.ReadProfile(fp_id);
  if (!font_profile.has_value()) {
    return base::ok(FailManagedFingerprintStartup(
        args, "fingerprint_load_failed",
        "failed to load fingerprint profile: " + font_profile.error()));
  }
  std::optional<std::vector<std::string>> installed_families;
#if BUILDFLAG(IS_WIN)
  if (IsWindowsFingerprint(font_profile->response.fingerprint.os,
                           font_profile->response.fingerprint.platform)) {
    installed_families = GetInstalledFontFamilies();
  }
#endif
  RuntimeFontSelection runtime_fonts = SelectRuntimeFonts(
      font_profile->response.fingerprint.fonts,
      font_profile->response.fingerprint.os,
      font_profile->response.fingerprint.platform, BUILDFLAG(IS_WIN),
      installed_families ? &*installed_families : nullptr);
  if (runtime_fonts.fonts != font_profile->response.fingerprint.fonts ||
      font_profile->request.runtime_font_catalog != runtime_fonts.catalog_id) {
    font_profile->response.fingerprint.fonts = std::move(runtime_fonts.fonts);
    font_profile->request.runtime_font_catalog =
        std::move(runtime_fonts.catalog_id);
    auto saved = profile_manager.SaveProfile(fp_id, *font_profile);
    if (!saved.has_value()) {
      return base::ok(FailManagedFingerprintStartup(
          args, "font_profile_save_failed", saved.error()));
    }
  }
#endif
  // Load fingerprint into accessor
  base::FilePath fp_path = profile_manager.GetFingerprintPath(fp_id);
  auto load_result = LoadFingerprint(fp_path);
  if (!load_result.has_value()) {
    return base::ok(FailManagedFingerprintStartup(
        args, "fingerprint_load_failed",
        "failed to load fingerprint profile: " + load_result.error()));
  }
  // The fingerprint's surface_policy is the backend's statement of which
  // surfaces this profile wants spoofed, so it supplies the default. Requiring
  // an extra --enable-*-spoofing switch on top of it meant a profile could ask
  // for canvas/WebGL spoofing and silently not get it -- WebGL in particular
  // would keep handing out the host's real adapter while the profile carried a
  // different renderer string.
  //
  // The switches remain an explicit local override in both directions:
  // --enable-* forces spoofing on for a profile that did not request it, and
  // --disable-* wins over everything.
  const RuntimeFingerprint* loaded = FingerprintAccessor::Get();
  FingerprintAccessor::SetSpoofingPolicy(
      ResolveSurfaceSpoofing(
          args.canvas_spoofing_enabled(), args.canvas_spoofing_suppressed(),
          loaded && loaded->surface_policy.canvas == "override"),
      // SwiftShader already supplies a host-independent vendor, renderer,
      // extension set, limits and pixels. Keep its native renderer string so
      // those surfaces cannot contradict one another, even while rolling out
      // against an older backend response that still requests an override.
      !IsSwiftShaderWebGLBackend(*command_line) &&
          ResolveSurfaceSpoofing(
              args.webgl_spoofing_enabled(),
              args.webgl_spoofing_suppressed(),
              loaded && loaded->surface_policy.webgl == "override"),
      command_line->HasSwitch(kExperimentalNormalizedCanvasSwitch));

  auto dev_proxy_result = ApplyDevProxyOverride();
  if (!dev_proxy_result.has_value()) {
    PrintError(args, "invalid_dev_proxy_url",
               "invalid CLAWBROWSER_DEV_PROXY_URL: " +
                   dev_proxy_result.error());
    result.should_exit = true;
    result.exit_code = 1;
    return base::ok(std::move(result));
  }
  std::optional<RuntimeProxyConfig> dev_proxy_override =
      std::move(*dev_proxy_result);
  const auto* proxy = FingerprintAccessor::GetProxy();
  if (args.require_proxy() && !proxy) {
    return base::ok(FailManagedFingerprintStartup(
        args, "required_proxy_missing",
        "profile launch requires a proxy but the fingerprint response did "
        "not include one"));
  }

  // Set command-line flags for Clawbrowser
  command_line->AppendSwitch(kRequireFingerprintSwitch);
  command_line->AppendSwitchPath(kFingerprintPathSwitch, fp_path);
  command_line->AppendSwitchPath(
      "user-data-dir", profile_manager.GetUserDataDir(fp_id));
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Linux renderers inherit the zygote sandbox before RendererMain and cannot
  // reopen arbitrary profile files. Pass the credential-stripped payload just
  // as on macOS, rather than weakening the sandbox or falling back to native.
  auto child_payload = dev_proxy_override.has_value()
                           ? BuildChildFingerprintPayload(fp_path,
                                                          *dev_proxy_override)
                           : BuildChildFingerprintPayload(fp_path);
  if (!child_payload.has_value()) {
    return base::ok(FailManagedFingerprintStartup(
        args, "fingerprint_child_payload_failed",
        "failed to prepare child fingerprint payload: " +
            child_payload.error()));
  }
  command_line->AppendSwitchASCII(kFingerprintChildDataSwitch, *child_payload);
#endif

  // Configure proxy flags
  if (proxy) {
    auto bridge_endpoint = PrepareProxyBridge(*proxy);
    if (!bridge_endpoint.has_value()) {
      PrintError(args, "proxy_bridge_failed",
                 "failed to start SOCKS5 auth bridge: " +
                     bridge_endpoint.error());
      result.should_exit = true;
      result.exit_code = 1;
      return base::ok(std::move(result));
    }

    auto proxy_flags = bridge_endpoint->has_value()
                           ? GetProxyCommandLineFlags(*proxy,
                                                      bridge_endpoint->value())
                           : GetProxyCommandLineFlags(*proxy);
    if (proxy_flags.empty()) {
      return base::ok(FailManagedFingerprintStartup(
          args, "invalid_proxy_config",
          "fingerprint response included an incomplete or unsupported proxy"));
    }
    for (const auto& flag : proxy_flags) {
      // Parse --key=value from flag string
      size_t eq = flag.find('=');
      if (eq != std::string::npos) {
        command_line->AppendSwitchASCII(
            flag.substr(2, eq - 2),  // strip leading --
            flag.substr(eq + 1));
      }
    }
  } else {
    StopSocks5AuthProxyBridge();
  }

  // Set language flags from fingerprint (Accept-Language header alignment)
  const auto* fp = FingerprintAccessor::Get();
  if (fp && !fp->user_agent.empty()) {
    command_line->AppendSwitchASCII(
        "user-agent", HeaderOrValue(*fp, "User-Agent", fp->user_agent));
  }
  if (fp && !fp->language.empty()) {
    // --lang sets the UI language
    command_line->AppendSwitchASCII("lang", fp->language[0]);
    // --accept-lang sets the Accept-Language HTTP header
    command_line->AppendSwitchASCII("accept-lang",
                                    AcceptLanguageFromFingerprint(*fp));
  }

  // Keep the real OS window inside the screen we claim to be on.
  // window.outerWidth/outerHeight are reported from the actual window and are
  // not spoofed, so a window wider than the advertised screen.availWidth is a
  // contradiction any detector can check in one line. Sizing the window from
  // the spoofed available area removes the mismatch at the source instead of
  // adding another lie on top of it.
  //
  // Explicit --window-size and --window-position values from the caller always
  // win; those are deliberate, controlled overrides. Otherwise anchor every
  // fingerprinted window at the virtual screen origin, including callers that
  // supply only a custom size.
  if (fp && !command_line->HasSwitch("window-size")) {
    const int avail_width =
        fp->screen.avail_width > 0 ? fp->screen.avail_width : fp->screen.width;
    const int avail_height = fp->screen.avail_height > 0
                                 ? fp->screen.avail_height
                                 : fp->screen.height;
    const WindowSize window =
        DeriveWindowSize(avail_width, avail_height,
                         static_cast<uint64_t>(fp->canvas_seed));
    if (window.width > 0 && window.height > 0) {
      command_line->AppendSwitchASCII(
          "window-size", base::NumberToString(window.width) + "," +
                             base::NumberToString(window.height));
    }
  }
  if (fp && !command_line->HasSwitch("window-position")) {
    command_line->AppendSwitchASCII("window-position", "0,0");
  }

  // Navigate to verify page on startup (unless --skip-verify)
  if (!args.skip_verify()) {
    command_line->AppendArg("clawbrowser://verify/");
  }

  return base::ok(std::move(result));
}

}  // namespace clawbrowser
