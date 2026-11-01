"""The login page, and quitting."""
from conftest import AUTH_RESPONSE, ENTER, ESC, RIGHT, TAB, Terminal


def test_login_sends_the_credentials_and_opens_the_chats_page(server, client):
    server.respond("POST", "/auth/login", 200, AUTH_RESPONSE)
    server.respond("GET", "/chats", 200, [])

    client.press("alice" + TAB + "password123" + ENTER)

    client.wait_for("New chat")
    request = server.requests_to("POST", "/auth/login")[0]
    assert request.body == {"username": "alice", "password": "password123"}


def test_later_requests_send_the_token(server, signed_in):
    assert server.requests_to("GET", "/chats")[0].authorization == "Bearer test-token"


def test_password_is_hidden_on_screen(client):
    client.press("alice" + TAB + "secret")

    client.wait_for("alice")
    assert "secret" not in client.text()


def test_register_button_posts_to_register(server, client):
    server.respond("POST", "/auth/register", 201, AUTH_RESPONSE)
    server.respond("GET", "/chats", 200, [])

    # Tab goes from password to Log in, and Right to Register.
    client.press("alice" + TAB + "password123" + TAB + RIGHT + ENTER)

    client.wait_for("New chat")
    assert len(server.requests_to("POST", "/auth/register")) == 1


def test_rejected_login_shows_the_server_error_and_stays(server, client):
    server.respond("POST", "/auth/login", 401, {"error": "Invalid username or password"})

    client.press("alice" + TAB + "wrong-password" + ENTER)

    client.wait_for("Invalid username or password")
    assert "New chat" not in client.text()


def test_unreachable_server_shows_an_error(server):
    server.stop()  # Nothing listens on this port anymore.
    terminal = Terminal(server.url)
    try:
        terminal.wait_for("Messenger")
        terminal.press("alice" + TAB + "password123" + ENTER)
        terminal.wait_for("Cannot reach the server")
    finally:
        terminal.close()


def test_esc_quits_cleanly(client):
    client.press(ESC)

    client.wait_for_exit()
    assert client.process.exitstatus == 0
