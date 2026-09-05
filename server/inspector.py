"""Field Inspector's restricted text and PCM bridge. Copyright Luke Steuber."""

import argparse
import base64
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import secrets
import sqlite3
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener

MAX_AUDIO = 128000  # 16 seconds, signed 8-bit mono PCM at 8 kHz
MAX_TEXT = 240
MAX_WORDS = 28
SYSTEM_PROMPT = (
    "You are Field Inspector, a resourceful pocket-sized field assistant with a playful "
    "gadget-adventure tone. Answer in one complete sentence of at most 20 words. "
    "Keep only the main point; omit secondary explanations. Use plain spoken language "
    "without markdown, under 240 characters. Never invent live facts "
    "or claim access to sensors, location, tools, or the internet. If information is "
    "missing, say so briefly. Do not use names, honorifics, greetings, or direct forms "
    "of address. No sound effects or impersonation."
)


class ApiError(Exception):
    def __init__(self, status, message):
        self.status, self.message = status, message


class Credentials:
    """Hashed installation credentials and persistent per-token request budgets."""

    def __init__(self, path):
        self.path = str(path)
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        with self.connect() as db:
            db.executescript("""
                CREATE TABLE IF NOT EXISTS tokens (
                    hash TEXT PRIMARY KEY, name TEXT NOT NULL, revoked INTEGER DEFAULT 0
                );
                CREATE TABLE IF NOT EXISTS usage (
                    token TEXT, bucket TEXT, count INTEGER,
                    PRIMARY KEY(token, bucket)
                );
            """)
        os.chmod(path, 0o600)

    @contextmanager
    def connect(self):
        # A sqlite context commits/rolls back but does not close the connection.
        database = sqlite3.connect(self.path, timeout=5)
        try:
            with database:
                yield database
        finally:
            database.close()

    @staticmethod
    def digest(token):
        return hashlib.sha256(token.encode()).hexdigest()

    def issue(self, name):
        token = secrets.token_urlsafe(32)
        with self.connect() as db:
            db.execute("INSERT INTO tokens(hash,name) VALUES(?,?)", (self.digest(token), name))
        return token

    def revoke(self, name):
        with self.connect() as db:
            return db.execute("UPDATE tokens SET revoked=1 WHERE name=?", (name,)).rowcount

    def authenticate(self, token):
        if not isinstance(token, str) or not 20 <= len(token) <= 128:
            raise ApiError(401, "Set the installation token in phone settings.")
        digest = self.digest(token)
        with self.connect() as db:
            row = db.execute("SELECT revoked FROM tokens WHERE hash=?", (digest,)).fetchone()
        if row is None or row[0]:
            raise ApiError(401, "Installation token is invalid or revoked.")
        return digest

    def consume(self, digest, now=None):
        now = time.time() if now is None else now
        buckets = [("minute:" + str(int(now // 60)), 8), ("day:" + str(int(now // 86400)), 60)]
        with self.connect() as db:
            db.execute("BEGIN IMMEDIATE")
            for bucket, limit in buckets:
                row = db.execute("SELECT count FROM usage WHERE token=? AND bucket=?", (digest, bucket)).fetchone()
                if row and row[0] >= limit:
                    raise ApiError(429, "Request limit reached. Try again later.")
            for bucket, _ in buckets:
                db.execute("INSERT INTO usage VALUES(?,?,1) ON CONFLICT(token,bucket) DO UPDATE SET count=count+1", (digest, bucket))
            # Only current windows matter; keep the budget database small.
            db.execute("DELETE FROM usage WHERE bucket NOT IN (?,?)", tuple(b[0] for b in buckets))


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, target):
        return None  # Keep credentials and questions on the configured gateway.


class Gateway:
    def __init__(self, base, key_file):
        if not isinstance(base, str):
            raise ValueError("Gateway must use HTTPS or loopback HTTP")
        parsed = urlsplit(base)
        port = parsed.port  # Also reject malformed or out-of-range ports.
        if (not parsed.hostname or parsed.username is not None or parsed.password is not None
                or parsed.query or parsed.fragment
                or not (parsed.scheme == "https" or
                        (parsed.scheme == "http" and parsed.hostname == "127.0.0.1" and port))):
            raise ValueError("Gateway must use HTTPS or loopback HTTP")
        self.base = base.rstrip("/")
        self.key_file = Path(key_file).expanduser()
        self.opener = build_opener(NoRedirect())

    def post(self, path, payload):
        key = self.key_file.read_text().strip()
        if not key:
            raise RuntimeError("Gateway credential unavailable")
        request = Request(self.base + path, data=json.dumps(payload).encode(), headers={
            "Content-Type": "application/json", "X-API-Key": key,
            "User-Agent": "FieldInspector/1.0",
        })
        with self.opener.open(request, timeout=40) as response:
            raw = response.read(2_000_001)
        if len(raw) > 2_000_000:
            raise ValueError("Oversized gateway response")
        return json.loads(raw)


def pcm_from_mp3(encoded):
    if not isinstance(encoded, str) or len(encoded) > 1_500_000:
        raise ValueError("Invalid speech payload")
    audio = base64.b64decode(encoded, validate=True)
    if not audio:
        raise ValueError("Empty speech payload")
    # Explicit MP3 input and pipe-only protocol prevent embedded playlist/URL reads.
    result = subprocess.run([
        "ffmpeg", "-nostdin", "-v", "error", "-protocol_whitelist", "pipe",
        "-f", "mp3", "-i", "pipe:0", "-vn", "-ac", "1", "-ar", "8000",
        "-t", "16.001", "-f", "s8", "pipe:1",
    ], input=audio, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=12, check=True)
    if not result.stdout or len(result.stdout) > MAX_AUDIO:
        raise ValueError("Speech exceeds watch limit")
    return result.stdout


class Inspector:
    def __init__(self, credentials, gateway, converter=pcm_from_mp3):
        self.credentials, self.gateway, self.converter = credentials, gateway, converter
        self.slots = threading.BoundedSemaphore(2)

    def inspect(self, token, data):
        digest = self.credentials.authenticate(token)
        if not isinstance(data, dict):
            raise ApiError(400, "Expected a question.")
        request_id, prompt, speak = data.get("request_id"), data.get("prompt"), data.get("speak", False)
        if type(request_id) is not int or not 0 <= request_id <= 2147483647:
            raise ApiError(400, "Invalid request number.")
        if not isinstance(prompt, str) or not prompt.strip() or len(prompt) > 400:
            raise ApiError(400, "Ask a question of 400 characters or fewer.")
        if type(speak) is not bool:
            raise ApiError(400, "Invalid speech setting.")
        if not self.slots.acquire(blocking=False):
            raise ApiError(503, "Inspector is busy. Try again shortly.")
        try:
            self.credentials.consume(digest)
            try:
                answer = self.gateway.post("/v1/llm/chat", {
                    "provider": "openai", "model": "gpt-4.1-mini", "max_tokens": 120,
                    "stream": False, "messages": [
                        {"role": "system", "content": SYSTEM_PROMPT},
                        {"role": "user", "content": prompt.strip()},
                    ],
                }).get("content")
                if not isinstance(answer, str) or not answer.strip():
                    raise ValueError("No answer")
                words = answer.split()
                answer = " ".join(words[:MAX_WORDS])
                if len(words) > MAX_WORDS or len(answer) > MAX_TEXT:
                    snippet = answer[:MAX_TEXT - 3]
                    if len(answer) > MAX_TEXT - 3 and " " in snippet:
                        snippet = snippet.rsplit(" ", 1)[0]
                    answer = snippet.rstrip() + "..."
            except Exception:
                raise ApiError(502, "Could not get an answer. Try again.") from None
            result = {"request_id": request_id, "text": answer, "audio": None}
            if speak:
                try:
                    speech = self.gateway.post("/v1/llm/speech", {
                        "text": answer, "provider": "openai", "model": "gpt-4o-mini-tts",
                        "voice": "echo", "speed": 1.05,
                    })
                    if speech.get("format") != "mp3":
                        raise ValueError("Unsupported speech format")
                    pcm = self.converter(speech.get("audio_data"))
                    if not pcm or len(pcm) > MAX_AUDIO:
                        raise ValueError("Invalid PCM length")
                    result["audio"] = {"pcm_base64": base64.b64encode(pcm).decode(), "sample_rate": 8000, "format": "s8"}
                except Exception:
                    result["warning"] = "Speech unavailable. The answer is ready to read."
            return result
        finally:
            self.slots.release()


class Handler(BaseHTTPRequestHandler):
    server_version = "FieldInspector/1.0"

    def setup(self):
        super().setup()
        self.connection.settimeout(15)

    def log_message(self, *_):
        pass  # No transcripts, headers, or request paths in access logs.

    def send_error(self, code, message=None, explain=None):
        # Parser failures and unsupported methods must use the same private
        # JSON envelope as application errors, without echoing request content.
        self.respond(code, {"error": "Request could not be processed."})

    def respond(self, status, data):
        body = json.dumps(data, ensure_ascii=True).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        try:
            if self.command != "HEAD":
                self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_GET(self):
        if self.path == "/health":
            self.respond(200, {"ok": True, "service": "field-inspector", "version": "1.0.0"})
        else:
            self.respond(404, {"error": "Not found."})

    def do_POST(self):
        if self.path != "/v1/inspect":
            self.respond(404, {"error": "Not found."})
            return
        try:
            authorization = self.headers.get("Authorization", "")
            token = authorization[7:] if authorization.startswith("Bearer ") else ""
            self.server.inspector.credentials.authenticate(token)
            if self.headers.get("Transfer-Encoding"):
                raise ApiError(400, "Use a JSON request with Content-Length.")
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 4096:
                raise ApiError(413, "Question is too large or empty.")
            if self.headers.get_content_type() != "application/json":
                raise ApiError(415, "Use application/json.")
            raw = self.rfile.read(length)
            if len(raw) != length:
                raise ApiError(400, "Incomplete JSON request.")
            data = json.loads(raw)
            self.respond(200, self.server.inspector.inspect(token, data))
        except ApiError as error:
            self.respond(error.status, {"error": error.message})
        except (ValueError, UnicodeError):
            self.respond(400, {"error": "Invalid JSON request."})
        except TimeoutError:
            self.respond(408, {"error": "Request timed out."})
        except Exception:
            self.respond(500, {"error": "Inspector is unavailable. Try again shortly."})


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, inspector):
        self.inspector = inspector
        super().__init__(address, Handler)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--state", default=os.environ.get("INSPECTOR_STATE", "~/.local/state/field-inspector/tokens.db"))
    parser.add_argument("--port", type=int, default=5055)
    parser.add_argument("--issue", metavar="NAME")
    parser.add_argument("--out", help="Private file for a new installation token")
    parser.add_argument("--revoke", metavar="NAME")
    args = parser.parse_args()
    credentials = Credentials(Path(args.state).expanduser())
    if args.issue:
        if not args.out:
            parser.error("--issue requires --out; tokens are never printed")
        path = Path(args.out).expanduser()
        # O_EXCL prevents overwriting a previously issued credential.
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "w") as output:
            output.write(credentials.issue(args.issue) + "\n")
        print("Installation token saved to", path)
        return
    if args.revoke:
        print("Revoked installations:", credentials.revoke(args.revoke))
        return
    gateway = Gateway(os.environ.get("DREAMER_API_BASE", "http://127.0.0.1:5200"),
                      os.environ.get("DREAMER_KEY_FILE", "~/.config/dreamer/key"))
    Server(("127.0.0.1", args.port), Inspector(credentials, gateway)).serve_forever()


if __name__ == "__main__":
    main()
