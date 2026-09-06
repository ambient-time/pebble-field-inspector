# Restricted Pebble endpoint

`inspector.py` is a Python standard-library service. It binds to loopback, calls the existing gateway, and converts its MP3 speech result with `ffmpeg`. It exposes only `GET /health` and authenticated `POST /v1/inspect`.

```json
{"request_id": 1, "prompt": "Why does a compass point north?", "speak": true}
```

Questions must contain at least one non-whitespace character and be no longer
than 400 characters, including surrounding whitespace.

Use `Authorization: Bearer <installation-token>` and `Content-Type: application/json`. A successful response contains `request_id`, a text answer of at most 28 words and 240 characters, and either `audio: null` or `{ "pcm_base64": "…", "sample_rate": 8000, "format": "s8" }`. PCM is mono, signed 8-bit, with a maximum of 128,000 bytes. Speech failure preserves the text reply and adds `warning`.

The caller cannot select upstream URLs, providers, models, system instructions, or a voice. The service uses `gpt-4.1-mini` for replies and `gpt-4o-mini-tts` with the `echo` voice. It does not expose arbitrary proxying or server-side tools.

## Run

Use Python 3.10 or newer and `ffmpeg`. No Python packages are required.

```sh
python3 server/inspector.py --port 5055
```

`DREAMER_API_BASE` defaults to `http://127.0.0.1:5200`; a remote gateway must use HTTPS. `DREAMER_KEY_FILE` defaults to `~/.config/dreamer/key`. Keep that gateway credential readable only by the service user. `INSPECTOR_STATE` defaults to `~/.local/state/field-inspector/tokens.db`.

The supplied systemd unit is an operator template. Adjust its user and paths for the host, create its writable state directory first, and route an HTTPS prefix to the loopback port. For the default phone endpoint, strip `/pebble-inspector` before forwarding:

```caddyfile
handle_path /pebble-inspector/* {
    reverse_proxy 127.0.0.1:5055
}
```

Back up routing configuration and run `caddy validate` before reloading. Never expose the loopback listener directly or insert a broad gateway credential in the Pebble bundle.

## Installation tokens

```sh
python3 server/inspector.py --issue time2 --out /private/path/time2-token.txt
python3 server/inspector.py --revoke time2
```

Issuance creates a new file with mode `0600` and refuses to overwrite an existing file. Only the token hash enters SQLite. Copy the token into the Pebble phone settings; it stays in the phone configuration and is not sent to the watch. Revocation takes effect on the next request. Reissuing a name does not revoke earlier tokens; revoke the name first when replacing a lost installation.

Usage counters survive service restarts. A single process admits at most two simultaneous upstream turns, with eight requests per minute and sixty per UTC day per token. Failed upstream attempts count against these limits. Do not run multiple service processes if relying on the in-process concurrency limit.

Run offline checks with `python3 -m unittest discover -s server -p 'test_*.py'`. Physical dictation, radio throughput, voice intelligibility, and wrist-button ergonomics still require real devices.
