import hashlib
import hmac
import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).parents[1] / "tools" / "phase6_recovery_mock.py"
SPEC = importlib.util.spec_from_file_location("phase6_recovery_mock", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class RecoveryMockTests(unittest.TestCase):
    def test_command_has_valid_hmac(self):
        key_hex = "11" * 32
        command = MODULE.build_command(
            "huerta-esp32-01", key_hex, "revoke_mqtt", 3, "request-003"
        )
        expected = hmac.new(
            bytes.fromhex(key_hex), MODULE.canonical_payload(command), hashlib.sha256
        ).hexdigest()
        self.assertEqual(command["authenticationTag"], expected)

    def test_action_changes_authenticated_payload(self):
        key_hex = "22" * 32
        revoke = MODULE.build_command(
            "huerta-esp32-01", key_hex, "revoke_mqtt", 4, "request-004"
        )
        unlink = MODULE.build_command(
            "huerta-esp32-01", key_hex, "unlink", 4, "request-004"
        )
        self.assertNotEqual(revoke["authenticationTag"], unlink["authenticationTag"])

    def test_rejects_invalid_key_and_version(self):
        with self.assertRaises(ValueError):
            MODULE.build_command("device", "aa", "unlink", 1)
        with self.assertRaises(ValueError):
            MODULE.build_command("device", "aa" * 32, "unlink", 0)


if __name__ == "__main__":
    unittest.main()

