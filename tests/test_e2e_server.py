"""
End-to-end test: start the real monitor server, connect a TCP client exactly
like the ESP32 firmware does, and assert the JSON payload it streams contains
every block the device expects (hw, claude, forest, events) — including the
Claude 80% reminder firing through the events banner.

Runs fully headless: no Windows hardware sources are required (LHM/winsdk are
optional imports that degrade to empty), so the payload still streams with
zeroed hw + a live claude block computed from ~/.claude (or empty).

Run: python -m pytest tests/test_e2e_server.py -v
"""
import json
import os
import socket
import sys
import threading
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))


def _free_port() -> int:
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def _read_one_payload(host: str, port: int, timeout: float = 20.0) -> dict:
    """Connect like the firmware (send HELO, read newline-terminated JSON)."""
    deadline = time.time() + timeout
    last_err = None
    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=3) as c:
                c.sendall(b"HELO\n")
                c.settimeout(5)
                buf = b""
                while time.time() < deadline:
                    chunk = c.recv(4096)
                    if not chunk:
                        break
                    buf += chunk
                    if b"\n" in buf:
                        line = buf.split(b"\n", 1)[0]
                        return json.loads(line.decode("utf-8"))
        except (OSError, ValueError) as e:
            last_err = e
            time.sleep(0.4)
    raise AssertionError(f"no payload within {timeout}s (last: {last_err})")


@pytest.fixture(scope="module")
def server():
    """Start monitor.main_async on a background event loop bound to a free port."""
    import asyncio
    import monitor

    port = _free_port()
    monitor.TCP_HOST = "127.0.0.1"
    monitor.TCP_PORT = port
    # Force a Claude state that crosses the 80% reminder so the E2E asserts it.
    monitor.CLAUDE_DAILY_BUDGET = 1          # tiny budget -> window_pct pinned 100
    monitor.CLAUDE_WEEKLY_BUDGET = 1
    monitor.CLAUDE_ALERT_PCT = 80

    loop = asyncio.new_event_loop()
    ready = threading.Event()

    def run():
        asyncio.set_event_loop(loop)
        loop.create_task(monitor.run())   # monitor's main coroutine
        loop.call_later(0.5, ready.set)
        loop.run_forever()

    t = threading.Thread(target=run, daemon=True)
    t.start()
    ready.wait(10)
    # run() sleeps ~2s before binding, then does the initial poll; give it room.
    time.sleep(6)
    yield "127.0.0.1", port
    loop.call_soon_threadsafe(loop.stop)


def test_payload_has_all_blocks(server):
    host, port = server
    p = _read_one_payload(host, port)
    # Core hardware keys always present.
    for k in ("ct", "gt", "cl", "gl", "ru", "ra"):
        assert k in p, f"missing hw key {k}"
    # New blocks the device renders.
    assert "claude" in p and isinstance(p["claude"], dict)
    assert "forest" in p and isinstance(p["forest"], dict)
    assert "events" in p and isinstance(p["events"], dict)
    assert "sv" in p  # server version


def test_claude_block_shape(server):
    host, port = server
    p = _read_one_payload(host, port)
    c = p["claude"]
    for k in ("ok", "win", "wk", "tok"):
        assert k in c, f"claude missing {k}"


def test_claude_80pct_alert_fires(server):
    """With a 1-token budget, window/weekly pin to 100% so the reminder must
    surface in the events banner."""
    host, port = server
    p = _read_one_payload(host, port)
    ev = p["events"]
    # Either a Claude reminder is the banner, or (no ~/.claude on this machine)
    # claude is unavailable; assert the wiring, not the user's data.
    if p["claude"].get("ok"):
        assert ev["n"] >= 1
        assert "Claude" in ev["top"], f"expected Claude banner, got {ev['top']}"
    else:
        # No local Claude data -> no false alert (graceful).
        assert "Claude" not in ev.get("top", "")
