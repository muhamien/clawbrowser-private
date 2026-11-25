#include "clawbrowser/verify/verify_page.h"

#include <iterator>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "clawbrowser/cli/api_client.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/cli/profile_manager.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/grit/clawbrowser_verify_resources.h"
#include "clawbrowser/paths.h"
#include "clawbrowser/verify/proxy_expectation.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_contents.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace clawbrowser {

ProxyExpectation::ProxyExpectation() = default;
ProxyExpectation::ProxyExpectation(const ProxyExpectation&) = default;
ProxyExpectation& ProxyExpectation::operator=(const ProxyExpectation&) =
    default;
ProxyExpectation::ProxyExpectation(ProxyExpectation&&) = default;
ProxyExpectation& ProxyExpectation::operator=(ProxyExpectation&&) = default;
ProxyExpectation::~ProxyExpectation() = default;

namespace {

std::string SerializeJson(base::ListValue list) {
  std::string json = "[]";
  base::JSONWriter::Write(base::Value(std::move(list)), &json);
  return json;
}

base::DictValue SerializeMediaDevice(const RuntimeMediaDevice& device) {
  base::DictValue item;
  item.Set("kind", device.kind.value_or(std::string()));
  item.Set("label", device.label.value_or(std::string()));
  item.Set("device_id", device.device_id.value_or(std::string()));
  return item;
}

base::DictValue SerializePlugin(const RuntimePlugin& plugin) {
  base::DictValue item;
  item.Set("name", plugin.name.value_or(std::string()));
  item.Set("description", plugin.description.value_or(std::string()));
  item.Set("filename", plugin.filename.value_or(std::string()));
  return item;
}

}  // namespace

bool VerifyFailureExitEnabledForCommandLine(
    const base::CommandLine& command_line) {
  return command_line.HasSwitch("verify-automation");
}

int ManagedProxyPrivacyCapabilityForCommandLine(
    const base::CommandLine& command_line,
    bool fingerprint_proxy_loaded) {
  constexpr char kRequiredWebRtcPolicy[] = "disable_non_proxied_udp";
  if (!fingerprint_proxy_loaded ||
      !command_line.HasSwitch(kRequireProxySwitch) ||
      command_line.GetSwitchValueASCII("proxy-server").empty() ||
      command_line.GetSwitchValueASCII("webrtc-ip-handling-policy") !=
          kRequiredWebRtcPolicy ||
      command_line.GetSwitchValueASCII("force-webrtc-ip-handling-policy") !=
          kRequiredWebRtcPolicy ||
      command_line.HasSwitch("no-proxy-server") ||
      command_line.HasSwitch("proxy-pac-url") ||
      command_line.HasSwitch("proxy-auto-detect") ||
      // This subtractive rule closes Chromium's implicit loopback/link-local
      // bypass. It is required, not a user exception; all other lists fail.
      command_line.GetSwitchValueASCII("proxy-bypass-list") != "<-loopback>") {
    return 0;
  }

  return 2;
}

VerifyPageUI::VerifyPageUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  automation_mode_ = VerifyFailureExitEnabledForCommandLine(
      *base::CommandLine::ForCurrentProcess());
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(), kVerifyHost);
  source->SetSupportedScheme("clawbrowser");
  SetupDataSource(source);
  web_ui->AddRequestableScheme("clawbrowser");

  web_ui->RegisterMessageCallback(
      "verifyProxy",
      base::BindRepeating(&VerifyPageUI::HandleVerifyProxy,
                          base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "verifyComplete",
      base::BindRepeating(&VerifyPageUI::HandleVerifyComplete,
                          base::Unretained(this)));
}

VerifyPageUI::~VerifyPageUI() = default;

void VerifyPageUI::SetupDataSource(content::WebUIDataSource* source) {
  // Resources are embedded at compile time via grit/grd
  source->AddResourcePath("verify.html", IDR_CLAWBROWSER_VERIFY_HTML);
  source->AddResourcePath("verify_timezones.js",
                          IDR_CLAWBROWSER_VERIFY_TIMEZONES_JS);
  source->AddResourcePath("verify.js", IDR_CLAWBROWSER_VERIFY_JS);
  source->AddResourcePath("verify.css", IDR_CLAWBROWSER_VERIFY_CSS);
  source->AddResourcePath("side-bite.svg", IDR_CLAWBROWSER_SIDE_BITE_SVG);
  source->SetDefaultResource(IDR_CLAWBROWSER_VERIFY_HTML);

  source->AddString("has_expected_values", "false");
  source->AddString("has_proxy", "false");
  source->AddString("user_agent", "");
  source->AddString("platform", "");
  source->AddString("language_primary", "");
  source->AddString("languages_json", "[]");
  source->AddString("hardware_concurrency", "0");
  source->AddString("device_memory", "0");
  source->AddString("screen_width", "0");
  source->AddString("screen_height", "0");
  source->AddString("screen_avail_width", "0");
  source->AddString("screen_avail_height", "0");
  source->AddString("screen_color_depth", "0");
  source->AddString("pixel_ratio", "0");
  source->AddString("timezone", "");
  source->AddString("canvas_policy", "native");
  source->AddString("canvas_spoofing_enabled", "false");
  source->AddString("webgl_policy", "native");
  source->AddString("webgl_spoofing_enabled", "false");
  source->AddString("webgl_vendor", "");
  source->AddString("webgl_renderer", "");
  source->AddString("fonts", "[]");
  source->AddString("media_devices_json", "");
  source->AddString("media_devices_count", "0");
  source->AddString("plugins_json", "");
  source->AddString("plugins_count", "0");
  source->AddString("battery_charging", "");
  source->AddString("battery_level", "");
  source->AddString("speech_voices_json", "");
  source->AddString("speech_voices_count", "0");
  source->AddString("managed_proxy_privacy", "0");

  // Inject expected fingerprint values as replacements in the HTML.
  const RuntimeFingerprint* fp = FingerprintAccessor::Get();
  if (!fp)
    return;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const int managed_proxy_privacy =
      command_line
          ? ManagedProxyPrivacyCapabilityForCommandLine(
                *command_line, FingerprintAccessor::GetProxy() != nullptr)
          : 0;
  source->AddString("managed_proxy_privacy",
                    base::NumberToString(managed_proxy_privacy));

  source->AddString("has_expected_values", "true");
  source->AddString("has_proxy",
                    FingerprintAccessor::GetProxy() ? "true" : "false");
  source->AddString("user_agent", fp->user_agent);
  source->AddString("platform", fp->platform);
  source->AddString("language_primary",
                     fp->language.empty() ? "" : fp->language[0]);

  // Languages as JSON array string for comparison
  base::ListValue lang_list;
  for (const auto& lang : fp->language)
    lang_list.Append(lang);
  std::string languages_json;
  base::JSONWriter::Write(base::Value(std::move(lang_list)),
                          &languages_json);
  source->AddString("languages_json", languages_json);

  source->AddString("hardware_concurrency",
                     base::NumberToString(fp->hardware.concurrency));
  source->AddString("device_memory",
                     base::NumberToString(fp->hardware.memory));
  source->AddString("screen_width",
                     base::NumberToString(fp->screen.width));
  source->AddString("screen_height",
                     base::NumberToString(fp->screen.height));
  source->AddString("screen_avail_width",
                     base::NumberToString(fp->screen.avail_width));
  source->AddString("screen_avail_height",
                     base::NumberToString(fp->screen.avail_height));
  source->AddString("screen_color_depth",
                     base::NumberToString(fp->screen.color_depth));
  source->AddString("pixel_ratio",
                     base::NumberToString(fp->screen.pixel_ratio));
  source->AddString("timezone", fp->timezone);
  source->AddString("canvas_policy", fp->surface_policy.canvas);
  source->AddString("canvas_spoofing_enabled",
                    fp->canvas_spoofing_enabled ? "true" : "false");
  source->AddString("webgl_policy", fp->surface_policy.webgl);
  source->AddString("webgl_spoofing_enabled",
                    fp->webgl_spoofing_enabled ? "true" : "false");
  source->AddString("webgl_vendor", fp->webgl.vendor);
  source->AddString("webgl_renderer", fp->webgl.renderer);

  // Fonts as JSON array
  base::ListValue fonts_list;
  for (const auto& font : fp->fonts)
    fonts_list.Append(font);
  source->AddString("fonts", SerializeJson(std::move(fonts_list)));

  base::ListValue media_devices_list;
  for (const auto& device : fp->media_devices)
    media_devices_list.Append(SerializeMediaDevice(device));
  if (!fp->media_devices.empty()) {
    source->AddString("media_devices_json",
                       SerializeJson(std::move(media_devices_list)));
  }

  source->AddString("media_devices_count",
                     base::NumberToString(fp->media_devices.size()));

  base::ListValue plugins_list;
  for (const auto& plugin : fp->plugins)
    plugins_list.Append(SerializePlugin(plugin));
  if (!fp->plugins.empty())
    source->AddString("plugins_json", SerializeJson(std::move(plugins_list)));

  source->AddString("plugins_count",
                     base::NumberToString(fp->plugins.size()));

  if (fp->battery) {
    if (fp->battery->charging.has_value()) {
      source->AddString("battery_charging",
                        *fp->battery->charging ? "true" : "false");
    }
    if (fp->battery->level.has_value()) {
      source->AddString("battery_level",
                        base::NumberToString(*fp->battery->level));
    }
  }

  base::ListValue speech_voices_list;
  for (const auto& voice : fp->speech_voices)
    speech_voices_list.Append(voice);
  if (!fp->speech_voices.empty()) {
    source->AddString("speech_voices_json",
                       SerializeJson(std::move(speech_voices_list)));
  }

  source->AddString("speech_voices_count",
                     base::NumberToString(fp->speech_voices.size()));
}

void VerifyPageUI::HandleVerifyProxy(const base::ListValue& args) {
  base::DictValue result;
  const bool proxy_configured = FingerprintAccessor::GetProxy() != nullptr;
  result.Set("match", !proxy_configured);
  result.Set("actual_country", "N/A");
  result.Set("verification_scope", "local_configuration");
  result.Set("detail", proxy_configured
      ? "proxy configured locally; public exit geography is unverified"
      : "no proxy configured");
  if (proxy_configured)
    result.Set("error_code", "external_verification_disabled");
  web_ui()->CallJavascriptFunctionUnsafe("onProxyVerifyResult",
                                         base::Value(std::move(result)));
}

bool ProxyHostRequiresClientGeography(const std::string& raw_host) {
  std::string host = base::ToLowerASCII(raw_host);
  if (!host.empty() && host.back() == '.')
    host.pop_back();
  if (host == "localhost" || base::EndsWith(host, ".localhost"))
    return true;
  if (host.size() > 2 && host.front() == '[' && host.back() == ']')
    host = host.substr(1, host.size() - 2);
  net::IPAddress address;
  return address.AssignFromIPLiteral(host) && !address.IsPubliclyRoutable();
}

std::optional<base::DictValue> ClientProxyGeoVerificationResult(
    const ClientProxyGeo& expected, const base::DictValue& observed) {
  const std::string* ip = observed.FindString("ip");
  const std::string* country = observed.FindString("country_code");
  if (!country)
    country = observed.FindString("country");
  const std::string* timezone = observed.FindString("timezone");
  net::IPAddress address, expected_address;
  if (!ip || !country || country->size() != 2 || !timezone || timezone->empty() ||
      !address.AssignFromIPLiteral(*ip) || !address.IsPubliclyRoutable() ||
      !expected_address.AssignFromIPLiteral(expected.ip)) {
    return std::nullopt;
  }
  auto canonical_timezone = [](const std::string& value) {
    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString canonical;
    icu::TimeZone::getCanonicalID(icu::UnicodeString::fromUTF8(value), canonical,
                                status);
    return U_SUCCESS(status) ? canonical : icu::UnicodeString();
  };
  const auto observed_timezone = canonical_timezone(*timezone);
  const bool match = address == expected_address && *country == expected.country &&
                     !observed_timezone.isEmpty() &&
                     observed_timezone == canonical_timezone(expected.timezone);
  base::DictValue result;
  result.Set("match", match);
  result.Set("actual_country", *country);
  result.Set("expected_country", expected.country);
  result.Set(address.IsIPv4() ? "ipv4" : "ipv6", *ip);
  result.Set("detail", match ? "browser exit matches the fingerprint geography"
                             : "browser exit geography changed; restart the profile");
  if (!match)
    result.Set("error_code", "client_proxy_geo_mismatch");
  return result;
}

void VerifyPageUI::HandleVerifyComplete(const base::ListValue& args) {
  if (args.empty() || !args[0].is_string())
    return;

  verification_complete_ = true;
  verify_passed_ = (args[0].GetString() == "pass");
  failure_exit_timer_.Stop();
  if (!verify_passed_)
    StartFailureExitTimer();
}

void VerifyPageUI::StartFailureExitTimer() {
  if (!automation_mode_ || !verification_complete_ || verify_passed_)
    return;

  // Spec: if verify fails and no CDP client connects within 30s, exit(1).
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(
      web_ui()->GetWebContents());
  if (agent_host && agent_host->IsAttached()) {
    // CDP client is connected — don't force exit, let client handle it
    return;
  }

  failure_exit_timer_.Start(
      FROM_HERE, base::Seconds(30),
      base::BindOnce(&VerifyPageUI::OnFailureExitTimeout,
                     base::Unretained(this)));
}

void VerifyPageUI::OnFailureExitTimeout() {
  if (!automation_mode_) {
    return;
  }

  // 30s elapsed, verify failed, no CDP client connected
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(
      web_ui()->GetWebContents());
  if (agent_host && agent_host->IsAttached()) {
    return;  // Client connected in the meantime
  }

  LOG(ERROR) << "[clawbrowser] verify failed, no CDP client connected "
             << "within 30s — exiting";
  base::Process::TerminateCurrentProcessImmediately(1);
}

}  // namespace clawbrowser
