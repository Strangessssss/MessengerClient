"""Shared fixtures. Each test runs the real client in a pseudo-terminal, against a fake server written here."""
import json
import os
import threading
import time
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import pyte
import pytest

if os.name == "nt":
    from winpty import PtyProcess
else:
    from ptyprocess import PtyProcessUnicode as PtyProcess

REPO_ROOT = Path(__file__).resolve().parents[2]
ROWS, COLUMNS = 40, 120
WAIT_SECONDS = 5

# What the real server answers to a sign in.
AUTH_RESPONSE = {"userId": 1, "token": "test-token"}

# What pressing these keys sends through a terminal.
ENTER = "\r"
TAB = "\t"
ESC = "\x1b"
DOWN = "\x1b[B"
RIGHT = "\x1b[C"
CTRL_D = "\x04"
CTRL_O = "\x0f"
CTRL_W = "\x17"


def message(id, seq, text, sender=1, status="sent"):
    """A message as the real server sends it. User 1 is alice, who the tests sign in as."""
    return {"id": id, "seq": seq, "chatId": "c1", "senderId": sender,
            "text": text, "status": status, "createdAt": 1700000000000}


def chat_view(*messages):
    """The answer to GET /chats/c1."""
    return {"chatId": "c1", "partnerUsername": "bob", "messages": list(messages)}


def chat_summary(unread=0, latest=None):
    """One entry of the answer to GET /chats."""
    return {"chatId": "c1", "partnerUsername": "bob", "unreadCount": unread,
            "latestMessage": None if latest is None else
            {"text": latest, "createdAt": 1700000000000, "status": "sent"}}


def find_client_binary():
    """Returns MESSENGER_CLIENT_BIN if set, or else the newest binary in build/ and cmake-build-*/."""
    configured = os.environ.get("MESSENGER_CLIENT_BIN")
    if configured:
        return Path(configured)

    names = ("MessengerClient", "MessengerClient.exe")
    build_dirs = [REPO_ROOT / "build", *REPO_ROOT.glob("cmake-build-*")]
    binaries = [folder / name for folder in build_dirs for name in names if (folder / name).is_file()]
    if not binaries:
        pytest.fail("Client binary not found. Build it first, or set MESSENGER_CLIENT_BIN.")
    return max(binaries, key=lambda path: path.stat().st_mtime)


@dataclass
class Request:
    method: str
    path: str
    authorization: str | None
    body: dict | None


class FakeServer:
    """Answers each route with what the test set, and records what the client asked.

    A test decides exactly what the server says, so it can also try answers the real server
    rarely or never gives. Routes without an answer get a 404.
    """

    def __init__(self):
        self.lock = threading.Lock()
        self.responses = {}
        self.requests = []
        self.stopped = False
        self.http = ThreadingHTTPServer(("127.0.0.1", 0), self._make_handler())
        self.url = f"http://127.0.0.1:{self.http.server_address[1]}"
        threading.Thread(target=self.http.serve_forever, daemon=True).start()

    def respond(self, method, path, status, body=None):
        """Sets the answer to a route. A body that's a str is sent as it is, and anything else as JSON."""
        with self.lock:
            self.responses[method, path] = (status, body)

    def requests_to(self, method, path):
        with self.lock:
            return [r for r in self.requests if (r.method, r.path) == (method, path)]

    def stop(self):
        """Safe to call again, so a test can stop the server to simulate an outage, before the fixture's cleanup."""
        if not self.stopped:
            self.stopped = True
            self.http.shutdown()
            self.http.server_close()

    def _answer(self, handler):
        length = int(handler.headers.get("Content-Length") or 0)
        raw = handler.rfile.read(length)
        with self.lock:
            self.requests.append(Request(
                handler.command,
                handler.path,
                handler.headers.get("Authorization"),
                json.loads(raw) if raw else None,
            ))
            status, body = self.responses.get((handler.command, handler.path), (404, {"error": "No such route"}))

        payload = b"" if body is None else (body if isinstance(body, str) else json.dumps(body)).encode()
        handler.send_response(status)
        handler.send_header("Content-Type", "application/json")
        handler.send_header("Content-Length", str(len(payload)))
        handler.end_headers()
        handler.wfile.write(payload)

    def _make_handler(self):
        fake = self

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                fake._answer(self)

            do_POST = do_DELETE = do_GET

            def log_message(self, *args):
                pass  # Keeps the test output clean.

        return Handler


class Terminal:
    """Runs the client in a pseudo-terminal, and keeps a copy of its screen that tests can read.

    The client only draws when it thinks it has a real terminal, so a plain pipe won't do. It draws
    with escape codes, and pyte is a terminal emulator that turns those into rows of text.
    """

    def __init__(self, server_url):
        self.screen = pyte.Screen(COLUMNS, ROWS)
        self.stream = pyte.Stream(self.screen)
        self.lock = threading.Lock()
        self.process = PtyProcess.spawn(
            [str(find_client_binary())],
            env={**os.environ, "MESSENGER_SERVER_URL": server_url},
            dimensions=(ROWS, COLUMNS),
        )
        threading.Thread(target=self._read_output, daemon=True).start()

    def _read_output(self):
        while True:
            try:
                output = self.process.read()
            except EOFError:
                return
            with self.lock:
                self.stream.feed(output)

    def text(self):
        with self.lock:
            return "\n".join(line.rstrip() for line in self.screen.display)

    def press(self, keys):
        """Sends keys, or text to type. Follow it with wait_for, as the client reacts a moment later.

        Don't send more keys after Enter without waiting. On Windows, a key sent within milliseconds
        of an Enter was seen to arrive before it.
        """
        self.process.write(keys)

    def wait_for(self, expected, seconds=WAIT_SECONDS):
        """Waits until the screen shows the text. Never use sleep and check, as that's slow and still flaky."""
        self.wait_until(lambda: expected in self.text(), f"{expected!r} to appear", seconds)

    def wait_for_gone(self, expected, seconds=WAIT_SECONDS):
        self.wait_until(lambda: expected not in self.text(), f"{expected!r} to disappear", seconds)

    def wait_for_exit(self, seconds=WAIT_SECONDS):
        self.wait_until(lambda: not self.process.isalive(), "the client to exit", seconds)

    def wait_until(self, condition, description, seconds=WAIT_SECONDS):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if condition():
                return
            time.sleep(0.02)
        pytest.fail(f"Timed out waiting for {description}. The screen shows:\n{self.text()}")

    def close(self):
        if self.process.isalive():
            self.process.terminate(force=True)


@pytest.fixture
def server():
    fake = FakeServer()
    yield fake
    fake.stop()


@pytest.fixture
def client(server):
    """The client on its login page, connected to the fake server."""
    terminal = Terminal(server.url)
    try:
        terminal.wait_for("Messenger")
        yield terminal
    finally:
        terminal.close()


@pytest.fixture
def signed_in(server, client):
    """The client on its chats page, signed in as alice with no chats. Tests can change the /chats answer."""
    server.respond("POST", "/auth/login", 200, AUTH_RESPONSE)
    server.respond("GET", "/chats", 200, [])
    client.press("alice" + TAB + "password123" + ENTER)
    client.wait_for("New chat")
    return client


@pytest.fixture
def open_chat(server, signed_in):
    """The client with an empty chat with bob open, and the message field focused."""
    server.respond("POST", "/chats", 200, {"id": "c1", "userIds": [1, 2], "createdAt": 1700000000000})
    server.respond("GET", "/chats", 200, [chat_summary()])
    server.respond("GET", "/chats/c1", 200, chat_view())
    signed_in.press(TAB + "bob" + ENTER)  # With no chats, Tab moves to New chat.
    signed_in.wait_for("bob: (no messages)")
    return signed_in
