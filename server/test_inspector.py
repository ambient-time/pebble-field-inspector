"""Offline boundary and HTTP tests for Field Inspector. No paid requests."""

import base64
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
from http.client import HTTPConnection
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import math
from pathlib import Path
import shutil
import socket
import sqlite3
import struct
import subprocess
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch
from urllib.error import HTTPError
import wave

try:
    from . import inspector as bridge
except ImportError:
    import inspector as bridge


class Fixture(unittest.TestCase):
    def setUp(self):
        clock = patch.object(bridge.time, "time", return_value=100 * 86400)
        clock.start()
        self.addCleanup(clock.stop)
        self.temporary = tempfile.TemporaryDirectory(prefix="field-inspector-test-")
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / "tokens.db"
        self.credentials = bridge.Credentials(self.path)
        self.token = self.credentials.issue("test watch")
        self.gateway = Mock()
        self.gateway.post.return_value = {"content": "Check the latch before moving."}
        self.converter = Mock(return_value=b"\x00\x10\xf0")
        self.inspector = bridge.Inspector(self.credentials, self.gateway, self.converter)

    def error(self, status, function, *args, **kwargs):
        with self.assertRaises(bridge.ApiError) as raised:
            function(*args, **kwargs)
        self.assertEqual(raised.exception.status, status)
        return raised.exception

    def ask(self, **changes):
        payload = {"request_id": 7, "prompt": "  What should I check?  ", "speak": False}
        payload.update(changes)
        return self.inspector.inspect(self.token, payload)


class CredentialTests(Fixture):
    def test_credentials_are_hashed_private_and_revocable_by_installation(self):
        second = self.credentials.issue("test watch")
        other = self.credentials.issue("other watch")
        self.assertNotIn(self.token.encode(), self.path.read_bytes())
        self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.credentials.authenticate(self.token), bridge.Credentials.digest(self.token))
        self.assertEqual(self.credentials.revoke("test watch"), 2)
        for token in (self.token, second):
            error = self.error(401, self.credentials.authenticate, token)
            self.assertNotIn(token, error.message)
        self.credentials.authenticate(other)

    def test_unknown_missing_short_and_oversized_tokens_are_rejected(self):
        for token in (None, 12, "", "short", "x" * 20, "x" * 129):
            with self.subTest(token_type=type(token).__name__):
                self.error(401, self.credentials.authenticate, token)

    def test_minute_budget_is_atomic_and_a_rejection_does_not_spend_day_budget(self):
        digest = self.credentials.authenticate(self.token)
        now = 100 * 86400
        def consume(_):
            try:
                self.credentials.consume(digest, now)
                return 200
            except bridge.ApiError as error:
                return error.status
        with ThreadPoolExecutor(max_workers=12) as pool:
            outcomes = list(pool.map(consume, range(20)))
        self.assertEqual(outcomes.count(200), 8)
        self.assertEqual(outcomes.count(429), 12)
        with closing(sqlite3.connect(self.path)) as database:
            count = database.execute("SELECT count FROM usage WHERE token=? AND bucket=?",
                                     (digest, "day:100")).fetchone()[0]
        self.assertEqual(count, 8)
        self.credentials.consume(digest, now + 60)

    def test_daily_budget_survives_restart_and_resets_next_day(self):
        digest = self.credentials.authenticate(self.token)
        now = 100 * 86400
        for number in range(60):
            self.credentials.consume(digest, now + (number // 8) * 60)
        restarted = bridge.Credentials(self.path)
        restarted.authenticate(self.token)
        self.error(429, restarted.consume, digest, now + 3600)
        restarted.consume(digest, now + 86400)

    def test_different_installations_have_independent_budgets(self):
        one = self.credentials.authenticate(self.token)
        two = self.credentials.authenticate(self.credentials.issue("another"))
        for _ in range(8):
            self.credentials.consume(one, 864000)
        self.error(429, self.credentials.consume, one, 864000)
        self.credentials.consume(two, 864000)


class InspectorTests(Fixture):
    def test_text_only_preserves_request_id_and_never_requests_speech(self):
        result = self.ask()
        self.assertEqual(result, {"request_id": 7, "text": "Check the latch before moving.", "audio": None})
        self.gateway.post.assert_called_once()
        path, payload = self.gateway.post.call_args.args
        self.assertEqual(path, "/v1/llm/chat")
        self.assertEqual(payload["messages"][-1], {"role": "user", "content": "What should I check?"})
        self.assertEqual(payload["provider"], "openai")
        self.assertEqual(payload["max_tokens"], 120)
        self.assertFalse(payload["stream"])
        self.converter.assert_not_called()

    def test_unknown_options_cannot_choose_gateway_provider_model_or_path(self):
        self.ask(provider="other", model="expensive", path="/anything", max_tokens=100000)
        path, payload = self.gateway.post.call_args.args
        self.assertEqual(path, "/v1/llm/chat")
        self.assertEqual(payload["model"], "gpt-4.1-mini")
        self.assertEqual(payload["max_tokens"], 120)

    def test_bad_inputs_fail_before_paid_request_or_budget_consumption(self):
        bad = [None, [], {}, {"request_id": True, "prompt": "question"},
               {"request_id": -1, "prompt": "question"},
               {"request_id": 2147483648, "prompt": "question"},
               {"request_id": 1, "prompt": " "},
               {"request_id": 1, "prompt": "x" * 401},
               {"request_id": 1, "prompt": []},
               {"request_id": 1, "prompt": "question", "speak": 1}]
        for payload in bad:
            with self.subTest(payload=payload):
                self.error(400, self.inspector.inspect, self.token, payload)
        self.gateway.post.assert_not_called()
        for _ in range(8):
            self.ask()
        self.error(429, self.ask)

    def test_revoked_token_never_reaches_gateway(self):
        self.credentials.revoke("test watch")
        self.error(401, self.ask)
        self.gateway.post.assert_not_called()

    def test_gateway_errors_are_sanitized_and_slots_are_released(self):
        for response in (None, [], {}, {"content": " "}, {"content": 123}):
            with self.subTest(response=response):
                self.gateway.post.return_value = response
                self.error(502, self.ask)
        self.gateway.post.side_effect = RuntimeError("upstream credential secret")
        error = self.error(502, self.ask)
        self.assertNotIn("secret", error.message)
        self.gateway.post.side_effect = None
        self.gateway.post.return_value = {"content": "Recovered."}
        self.assertEqual(self.ask()["text"], "Recovered.")

    def test_failed_paid_attempts_still_consume_budget(self):
        self.gateway.post.side_effect = TimeoutError("paid upstream timed out")
        for _ in range(8):
            self.error(502, self.ask)
        self.error(429, self.ask)
        self.assertEqual(self.gateway.post.call_count, 8)

    def test_output_word_character_and_whitespace_caps(self):
        for text in ("short " * 80, "x" * 1000, "é" * 1000,
                     "\n\tUseful\r\n  answer.\t", "a " * 120):
            with self.subTest(text=text[:12]):
                self.gateway.post.return_value = {"content": text}
                answer = self.ask()["text"]
                self.assertLessEqual(len(answer), 240)
                self.assertLessEqual(len(answer.split()), 28)
                self.assertNotIn("\n", answer)
                self.assertTrue(answer.strip())

    def test_speech_success_has_the_watch_pcm_contract(self):
        self.gateway.post.side_effect = [{"content": "Use a light."},
                                         {"format": "mp3", "audio_data": "encoded"}]
        result = self.ask(speak=True)
        self.assertEqual(result["audio"], {"pcm_base64": "ABDw",
                                          "sample_rate": 8000, "format": "s8"})
        self.converter.assert_called_once_with("encoded")
        path, payload = self.gateway.post.call_args.args
        self.assertEqual(path, "/v1/llm/speech")
        self.assertEqual(payload["text"], result["text"])

    def test_speech_gateway_format_conversion_and_output_errors_keep_text(self):
        failures = [RuntimeError("private upstream failure"), {}, {"format": "wav"}]
        for speech in failures:
            with self.subTest(speech=repr(speech)):
                self.gateway.post.side_effect = [{"content": "Useful answer."}, speech]
                result = self.ask(speak=True)
                self.assertEqual(result["text"], "Useful answer.")
                self.assertIsNone(result["audio"])
                self.assertNotIn("private", result["warning"])
        for pcm in (b"", b"x" * (bridge.MAX_AUDIO + 1), "not bytes"):
            self.gateway.post.side_effect = [{"content": "Useful answer."},
                                             {"format": "mp3", "audio_data": "encoded"}]
            self.converter.return_value = pcm
            self.assertIsNone(self.ask(speak=True)["audio"])
        self.gateway.post.side_effect = [{"content": "Useful answer."},
                                         {"format": "mp3", "audio_data": "encoded"}]
        self.converter.side_effect = subprocess.TimeoutExpired("ffmpeg", 12)
        self.assertIsNone(self.ask(speak=True)["audio"])

    def test_exact_audio_cap_is_accepted(self):
        self.gateway.post.side_effect = [{"content": "Answer."},
                                         {"format": "mp3", "audio_data": "encoded"}]
        self.converter.return_value = b"\x00" * bridge.MAX_AUDIO
        result = self.ask(speak=True)
        self.assertEqual(len(base64.b64decode(result["audio"]["pcm_base64"])), bridge.MAX_AUDIO)

    def test_only_two_upstream_requests_can_run_and_busy_does_not_spend_budget(self):
        entered = threading.Barrier(3)
        release = threading.Event()
        def blocked_gateway(*_):
            entered.wait(timeout=5)
            self.assertTrue(release.wait(timeout=5))
            return {"content": "Done."}
        self.gateway.post.side_effect = blocked_gateway
        with ThreadPoolExecutor(max_workers=2) as pool:
            first, second = pool.submit(self.ask), pool.submit(self.ask)
            try:
                entered.wait(timeout=5)
                self.error(503, self.ask)
                self.assertEqual(self.gateway.post.call_count, 2)
            finally:
                release.set()
            first.result(timeout=5)
            second.result(timeout=5)
        self.gateway.post.side_effect = None
        self.gateway.post.return_value = {"content": "Available."}
        for _ in range(6):
            self.ask()
        self.error(429, self.ask)


class HttpTests(Fixture):
    def setUp(self):
        super().setUp()
        self.server = bridge.Server(("127.0.0.1", 0), self.inspector)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.close_server)

    def close_server(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)

    def request(self, method="POST", path="/v1/inspect", body=None, headers=None):
        if body is None:
            body = json.dumps({"request_id": 7, "prompt": "Question?"}).encode()
        actual = {"Authorization": "Bearer " + self.token, "Content-Type": "application/json"}
        actual.update(headers or {})
        connection = HTTPConnection(*self.server.server_address, timeout=3)
        try:
            connection.request(method, path, body=body, headers=actual)
            response = connection.getresponse()
            raw = response.read()
            return response.status, dict(response.getheaders()), raw
        finally:
            connection.close()

    def assert_private(self, response, status):
        code, headers, raw = response
        self.assertEqual(code, status)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(headers["X-Content-Type-Options"], "nosniff")
        self.assertEqual(headers["Content-Type"], "application/json; charset=utf-8")
        self.assertEqual(int(headers["Content-Length"]), len(raw))
        return json.loads(raw)

    def test_success_and_health_have_private_json_headers(self):
        self.assertEqual(self.assert_private(self.request(), 200)["request_id"], 7)
        self.assertTrue(self.assert_private(self.request("GET", "/health"), 200)["ok"])

    def test_authentication_and_revocation_status(self):
        self.assert_private(self.request(headers={"Authorization": ""}), 401)
        self.credentials.revoke("test watch")
        self.assert_private(self.request(), 401)
        self.gateway.post.assert_not_called()

    def test_bad_json_length_type_transfer_encoding_and_unknown_paths(self):
        cases = [(b"{", {}, 400), (b"\xff", {}, 400), (b"[]", {}, 400),
                 (b"{}", {"Content-Type": "text/plain"}, 415),
                 (b"{}", {"Content-Length": "4097"}, 413),
                 (b"", {"Content-Length": "0"}, 413),
                 (b"{}", {"Content-Length": "invalid"}, 400),
                 (b"{}", {"Transfer-Encoding": "chunked"}, 400)]
        for body, headers, status in cases:
            with self.subTest(status=status, headers=headers):
                self.assert_private(self.request(body=body, headers=headers), status)
        self.assert_private(self.request(path="/other"), 404)
        self.assert_private(self.request("GET", "/v1/inspect"), 404)
        self.gateway.post.assert_not_called()

    def test_real_http_budget_busy_and_gateway_error_status(self):
        self.inspector.slots.acquire()
        self.inspector.slots.acquire()
        try:
            self.assert_private(self.request(), 503)
        finally:
            self.inspector.slots.release()
            self.inspector.slots.release()
        self.gateway.post.side_effect = RuntimeError("upstream key must not escape")
        self.assert_private(self.request(), 502)
        self.gateway.post.side_effect = None
        for _ in range(7):
            self.assert_private(self.request(), 200)
        self.assert_private(self.request(), 429)

    def test_unsupported_methods_also_return_private_json_errors(self):
        self.assert_private(self.request("PUT"), 501)
        status, headers, raw = self.request("HEAD")
        self.assertEqual(status, 501)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(raw, b"")

    def test_truncated_declared_body_never_reaches_paid_gateway(self):
        body = b'{"request_id":7,"prompt":"Question?"}'
        with socket.create_connection(self.server.server_address, timeout=3) as client:
            request = ("POST /v1/inspect HTTP/1.0\r\nAuthorization: Bearer " + self.token +
                       "\r\nContent-Type: application/json\r\nContent-Length: " +
                       str(len(body) + 20) + "\r\n\r\n").encode() + body
            client.sendall(request)
            client.shutdown(socket.SHUT_WR)
            received = b""
            while chunk := client.recv(8192):
                received += chunk
        self.assertIn(b" 400 ", received.split(b"\r\n", 1)[0])
        self.assertIn(b"Cache-Control: no-store", received)
        self.gateway.post.assert_not_called()


class GatewayTests(unittest.TestCase):
    def test_base_url_accepts_only_unambiguous_https_or_loopback_http(self):
        for base in ("https://gateway.example", "http://127.0.0.1:5200"):
            bridge.Gateway(base, "/unused")
        for base in ("http://gateway.example", "http://127.0.0.1:5200@external.invalid",
                     "https://user:password@gateway.example", "https://", "https://gateway.example?query=1"):
            with self.subTest(base=base), self.assertRaises(ValueError):
                bridge.Gateway(base, "/unused")

    def test_gateway_response_size_and_missing_credentials_are_bounded(self):
        with tempfile.TemporaryDirectory() as directory:
            key = Path(directory) / "key"
            key.write_text("test-gateway-key")
            gateway = bridge.Gateway("https://gateway.example", key)
            with patch.object(gateway.opener, "open", return_value=io.BytesIO(b"x" * 2_000_001)) as upstream:
                with self.assertRaises(ValueError):
                    gateway.post("/v1/llm/chat", {"prompt": "question"})
                request = upstream.call_args.args[0]
                self.assertEqual(request.get_header("X-api-key"), "test-gateway-key")
            key.write_text("")
            with patch.object(gateway.opener, "open") as upstream:
                with self.assertRaises(RuntimeError):
                    gateway.post("/v1/llm/chat", {})
                upstream.assert_not_called()

    def test_redirects_never_forward_questions_or_credentials(self):
        seen = []
        class Redirect(BaseHTTPRequestHandler):
            def do_POST(self):
                seen.append(self.path)
                self.send_response(302)
                self.send_header("Location", "/unexpected-destination")
                self.send_header("Content-Length", "0")
                self.end_headers()

            def do_GET(self):
                seen.append(self.path)
                self.send_response(200)
                self.send_header("Content-Length", "2")
                self.end_headers()
                self.wfile.write(b"{}")

            def log_message(self, *_):
                pass

        server = ThreadingHTTPServer(("127.0.0.1", 0), Redirect)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with tempfile.TemporaryDirectory() as directory:
                key = Path(directory) / "key"
                key.write_text("private-test-key")
                gateway = bridge.Gateway("http://127.0.0.1:" + str(server.server_port), key)
                with self.assertRaises(HTTPError) as raised:
                    gateway.post("/v1/llm/chat", {"prompt": "private test question"})
                self.assertEqual(raised.exception.code, 302)
                raised.exception.close()
                self.assertEqual(seen, ["/v1/llm/chat"])
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)


class ConversionTests(unittest.TestCase):
    def test_invalid_payload_is_rejected_before_ffmpeg(self):
        with patch.object(bridge.subprocess, "run") as ffmpeg:
            for payload in (None, "", "!", "x" * 1_500_001):
                with self.subTest(payload_type=type(payload).__name__), self.assertRaises(ValueError):
                    bridge.pcm_from_mp3(payload)
            ffmpeg.assert_not_called()

    def test_empty_oversized_and_timed_out_conversion_are_rejected(self):
        encoded = base64.b64encode(b"pretend mp3").decode()
        for pcm in (b"", b"x" * (bridge.MAX_AUDIO + 1)):
            with patch.object(bridge.subprocess, "run", return_value=Mock(stdout=pcm)):
                with self.assertRaises(ValueError):
                    bridge.pcm_from_mp3(encoded)
        with patch.object(bridge.subprocess, "run", side_effect=subprocess.TimeoutExpired("ffmpeg", 12)):
            with self.assertRaises(subprocess.TimeoutExpired):
                bridge.pcm_from_mp3(encoded)

    @unittest.skipUnless(shutil.which("ffmpeg"), "ffmpeg is not installed")
    def test_real_mp3_conversion_returns_signed_mono_8khz_pcm(self):
        buffer = io.BytesIO()
        with wave.open(buffer, "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(16000)
            output.writeframes(b"".join(struct.pack("<h", int(12000 * math.sin(2 * math.pi * 440 * i / 16000)))
                                        for i in range(16000)))
        mp3 = subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-f", "wav", "-i", "pipe:0",
                              "-codec:a", "libmp3lame", "-f", "mp3", "pipe:1"], input=buffer.getvalue(),
                             capture_output=True, timeout=12, check=True).stdout
        pcm = bridge.pcm_from_mp3(base64.b64encode(mp3).decode())
        self.assertGreaterEqual(len(pcm), 7900)
        self.assertLess(len(pcm), 10000)  # MP3 padding can extend a streamed second.
        values = struct.unpack(str(len(pcm)) + "b", pcm)
        self.assertLess(min(values), 0)
        self.assertGreater(max(values), 0)
        crossings = [i for i in range(1, len(values)) if values[i - 1] < 0 <= values[i]]
        periods = [b - a for a, b in zip(crossings, crossings[1:]) if b - a < 30]
        self.assertGreater(len(periods), 400)
        self.assertAlmostEqual(sum(periods) / len(periods), 8000 / 440, delta=1)


if __name__ == "__main__":
    unittest.main()
