# ภาพรวม: ทดสอบตัวช่วย Spark เช่น UID แยกกัน URL ถูกต้อง และ escape ค่าใน header
import importlib.util
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("spark_setup", Path(__file__).resolve().parents[1] / "scripts/configure_firebase_device.py")
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)

class SparkSetupTests(unittest.TestCase):
    def setUp(self):
        self.config = dict(databaseURL="https://demo-default-rtdb.asia-southeast1.firebasedatabase.app/",
            apiKey="A" * 39, deviceEmail="device@example.com", devicePassword='test"password',
            deviceUid="deviceUID", ownerUid="ownerUID", deviceId="sl-4824d3")
    def test_device_owner_must_differ(self):
        self.config["deviceUid"] = self.config["ownerUid"]
        with self.assertRaises(ValueError): helper.validate_config(self.config)
    def test_reject_http_and_query_credentials(self):
        for url in ["http://demo.firebaseio.com", "https://demo.firebaseio.com/?auth=secret", "https://evil.example"]:
            with self.subTest(url=url):
                with self.assertRaises(ValueError): helper.validate_config({**self.config, "databaseURL": url})
    def test_header_escapes_password_and_has_spark_settings(self):
        config = helper.validate_config(self.config)
        text = helper.render_header(config)
        self.assertIn('test\\"password', text)
        self.assertNotIn("FIREBASE_SYNC_URL", text)
        self.assertIn("FIREBASE_DATABASE_URL", text)
        self.assertFalse(config["databaseURL"].endswith("/"))

if __name__ == "__main__": unittest.main()
