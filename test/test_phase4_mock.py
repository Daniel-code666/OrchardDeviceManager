import hashlib
import hmac
import unittest

from tools.phase4_provisioning_mock import build_response, canonical_payload


REQUEST = {
    "type": "claim_request",
    "contractVersion": 1,
    "deviceId": "huerta-esp32-test",
    "requestId": "0123456789abcdef0123456789abcdef",
    "claimCode": "ABCDEFGH2345",
    "expiresInSeconds": 300,
}


class Phase4MockTests(unittest.TestCase):
    def test_approved_response_is_bound_to_request_and_owner(self):
        response = build_response(REQUEST, "approved", "user-test", "garden-test")
        expected = hmac.new(
            REQUEST["claimCode"].encode(), canonical_payload(response), hashlib.sha256
        ).hexdigest()
        self.assertEqual(expected, response["authenticationTag"])

    def test_rejected_response_contains_no_owner(self):
        response = build_response(REQUEST, "rejected")
        self.assertEqual("", response["userId"])
        self.assertEqual("", response["gardenId"])
        self.assertEqual("claim_rejected", response["errorCode"])

    def test_invalid_authentication_changes_tag(self):
        valid = build_response(REQUEST, "approved", "user-test", "garden-test")
        invalid = build_response(
            REQUEST, "approved", "user-test", "garden-test", invalid_authentication=True
        )
        self.assertNotEqual(valid["authenticationTag"], invalid["authenticationTag"])


if __name__ == "__main__":
    unittest.main()

