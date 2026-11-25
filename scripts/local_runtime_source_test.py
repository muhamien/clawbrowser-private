"""Source regression checks. Browser execution and network isolation need separate tests."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class LocalRuntimeSourceTest(unittest.TestCase):
    def test_startup_has_no_vendor_acquisition(self):
        source = (ROOT / "clawbrowser/startup.cc").read_text()
        for forbidden in ("ResolveApiKey", "GenerateFingerprint", "ApiClient", "ConfigureAuthStartup"):
            self.assertNotIn(forbidden, source)
        self.assertIn('"local_profile_unavailable"', source)
        self.assertIn('"local_profile_update_required"', source)
        self.assertIn("profile_manager.ReadProfile(fp_id)", source)

    def test_api_boundary_has_no_network_transport(self):
        source = (ROOT / "clawbrowser/cli/api_client.cc").read_text()
        for forbidden in ("ResourceRequest", "SimpleURLLoader", "DownloadToString", "https://"):
            self.assertNotIn(forbidden, source)
        self.assertIn('"external_verification_disabled"', source)

    def test_verification_uses_local_inspection(self):
        source = (ROOT / "clawbrowser/verify/verify_page.cc").read_text()
        for forbidden in ("ipinfo.io", "ipapi.co", "DownloadToString", "VerifyClientProxyGeo", "VerifyProxy(request"):
            self.assertNotIn(forbidden, source)
        self.assertIn('"local_configuration"', source)

    def test_vendor_defaults_removed(self):
        source = (ROOT / "clawbrowser/BUILD.gn").read_text()
        self.assertNotIn("api.clawbrowser.ai", source)
        self.assertNotIn("api.qa.clawbrowser.ai", source)
        source = (ROOT / "clawbrowser/auth/resources/auth.html").read_text()
        self.assertNotIn("api-key", source)

if __name__ == "__main__":
    unittest.main()
