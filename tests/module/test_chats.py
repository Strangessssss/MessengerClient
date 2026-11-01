"""GET /chats, POST /chats, GET /chats/<id> and DELETE /chats/<id>, as a user sees them."""

from conftest import CTRL_D, CTRL_O, CTRL_W, ENTER, TAB, chat_summary, chat_view, message

# How long a test waits for something that arrives with a poll, which happens every 2 seconds.
POLL_SECONDS = 5


# GET /chats

def test_chats_list_shows_a_chat_without_messages(server, signed_in):
    server.respond("GET", "/chats", 200, [chat_summary()])

    signed_in.wait_for("bob: (no messages)", seconds=POLL_SECONDS)


def test_chats_list_shows_unread_count_and_latest_message(server, signed_in):
    server.respond("GET", "/chats", 200, [chat_summary(unread=2, latest="hello")])

    signed_in.wait_for("bob (2): hello", seconds=POLL_SECONDS)


def test_chats_list_hides_unread_count_of_the_open_chat(server, open_chat):
    server.respond("GET", "/chats", 200, [chat_summary(unread=2, latest="hello")])

    open_chat.wait_for("bob: hello", seconds=POLL_SECONDS)


def test_rejected_token_shows_the_error_and_keeps_running(server, signed_in):
    server.respond("GET", "/chats", 401, {"error": "Missing or invalid token"})

    signed_in.wait_for("Missing or invalid token", seconds=POLL_SECONDS)
    assert signed_in.process.isalive()


def test_lost_server_shows_the_error_and_keeps_running(server, signed_in):
    server.stop()

    signed_in.wait_for("Cannot reach the server", seconds=POLL_SECONDS)
    assert signed_in.process.isalive()


# POST /chats

def test_new_chat_sends_the_username_and_opens_the_chat(server, open_chat):
    request = server.requests_to("POST", "/chats")[0]
    assert request.body == {"username": "bob"}
    assert request.authorization == "Bearer test-token"
    assert server.requests_to("GET", "/chats/c1")


def test_new_chat_with_unknown_user_shows_the_server_error(server, signed_in):
    server.respond("POST", "/chats", 404, {"error": "User not found"})

    signed_in.press(TAB + "ghost" + ENTER)

    signed_in.wait_for("User not found")


def test_new_chat_without_a_username_sends_nothing(server, signed_in):
    signed_in.press(TAB + ENTER)

    signed_in.wait_for("Enter the username of a user")
    assert server.requests_to("POST", "/chats") == []


def test_new_chat_with_a_wrong_shaped_answer_shows_an_error(server, signed_in):
    server.respond("POST", "/chats", 200, "[]")

    signed_in.press(TAB + "bob" + ENTER)

    signed_in.wait_for("Unexpected response from the server")


# GET /chats/<id>

def test_enter_on_a_chat_opens_it_and_sends_the_token(server, signed_in):
    server.respond("GET", "/chats", 200, [chat_summary(unread=1, latest="from bob")])
    server.respond("GET", "/chats/c1", 200, chat_view(message("m1", 1, "from bob", sender=2)))
    signed_in.wait_for("bob (1): from bob", seconds=POLL_SECONDS)

    signed_in.press(ENTER)

    signed_in.wait_for_gone("Select a chat")
    assert server.requests_to("GET", "/chats/c1")[0].authorization == "Bearer test-token"


def test_chat_that_cannot_be_opened_shows_the_server_error(server, signed_in):
    server.respond("GET", "/chats", 200, [chat_summary()])
    server.respond("GET", "/chats/c1", 404, {"error": "Chat not found"})
    signed_in.wait_for("bob: (no messages)", seconds=POLL_SECONDS)

    signed_in.press(ENTER)

    signed_in.wait_for("Chat not found")


# Closing, which needs no request

def test_ctrl_w_closes_the_chat_and_keeps_it_in_the_list(open_chat):
    open_chat.press(CTRL_W)

    open_chat.wait_for("Select a chat")
    assert "bob: (no messages)" in open_chat.text()


# DELETE /chats/<id>

def test_first_ctrl_d_only_asks_for_confirmation(server, open_chat):
    server.respond("DELETE", "/chats/c1", 204)

    open_chat.press(CTRL_D)

    open_chat.wait_for("Press Ctrl+D again")
    assert server.requests_to("DELETE", "/chats/c1") == []


def test_second_ctrl_d_deletes_the_chat(server, open_chat):
    server.respond("DELETE", "/chats/c1", 204)
    open_chat.press(CTRL_D)
    open_chat.wait_for("Press Ctrl+D again")
    server.respond("GET", "/chats", 200, [])

    open_chat.press(CTRL_D)

    open_chat.wait_for("Select a chat")
    open_chat.wait_for_gone("bob")
    requests = server.requests_to("DELETE", "/chats/c1")
    assert len(requests) == 1
    assert requests[0].authorization == "Bearer test-token"


def test_any_other_key_cancels_the_delete(server, open_chat):
    open_chat.press(CTRL_D)
    open_chat.wait_for("Press Ctrl+D again")

    open_chat.press("x")

    open_chat.wait_for_gone("Press Ctrl+D again")
    assert server.requests_to("DELETE", "/chats/c1") == []


def test_failed_delete_shows_the_server_error_and_keeps_the_chat_open(server, open_chat):
    server.respond("DELETE", "/chats/c1", 404, {"error": "Chat not found"})
    open_chat.press(CTRL_D)
    open_chat.wait_for("Press Ctrl+D again")

    open_chat.press(CTRL_D)

    open_chat.wait_for("Chat not found")
    assert "Select a chat" not in open_chat.text()


def test_chat_deleted_by_the_other_user_closes(server, open_chat):
    server.respond("GET", "/chats", 200, [])

    open_chat.wait_for("Select a chat", seconds=POLL_SECONDS)


# Signing out, which forgets the token

def test_ctrl_o_returns_to_the_login_page(open_chat):
    open_chat.press(CTRL_O)

    open_chat.wait_for("Log in")
    open_chat.wait_for_gone("New chat")  # It's lower on the screen, so a redraw reaches it after "Log in".
