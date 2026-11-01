"""POST /chats/<id>/messages, and new messages arriving through GET /chats/<id>."""
from conftest import ENTER, chat_view, message

# How long a test waits for something that arrives with a poll, which happens every 2 seconds.
POLL_SECONDS = 5
PLACEHOLDER = "Type a message"


def test_send_posts_the_text_and_shows_the_message(server, open_chat):
    server.respond("POST", "/chats/c1/messages", 201, message("m1", 1, "hello bob"))

    open_chat.press("hello bob" + ENTER)

    open_chat.wait_for("You")
    open_chat.wait_for("sent")
    request = server.requests_to("POST", "/chats/c1/messages")[0]
    assert request.body == {"text": "hello bob"}
    assert request.authorization == "Bearer test-token"


def test_send_clears_the_message_field(server, open_chat):
    server.respond("POST", "/chats/c1/messages", 201, message("m1", 1, "hello bob"))

    open_chat.press("hello bob" + ENTER)

    open_chat.wait_for("You")
    open_chat.wait_for(PLACEHOLDER)


def test_empty_message_sends_nothing(server, open_chat):
    polls_before = len(server.requests_to("GET", "/chats/c1"))
    open_chat.press(ENTER)
    # Keys sent right after Enter can overtake it, so let a poll pass before typing again.
    open_chat.wait_until(
        lambda: len(server.requests_to("GET", "/chats/c1")) > polls_before,
        "the next poll",
        POLL_SECONDS,
    )
    # The field accepts this text, which proves the client is still running and has handled the Enter.
    open_chat.press("typed later")

    open_chat.wait_for("typed later")
    assert server.requests_to("POST", "/chats/c1/messages") == []


def test_rejected_message_shows_the_error_and_keeps_the_text(server, open_chat):
    server.respond("POST", "/chats/c1/messages", 400, {"error": "Message was rejected"})

    open_chat.press("keep me" + ENTER)

    open_chat.wait_for("Message was rejected")
    assert "keep me" in open_chat.text()


def test_unreachable_server_keeps_the_text_to_send_again(server, open_chat):
    server.stop()

    open_chat.press("keep me" + ENTER)

    open_chat.wait_for("Cannot reach the server")
    assert "keep me" in open_chat.text()


def test_incoming_message_appears_without_any_key(server, open_chat):
    server.respond("GET", "/chats/c1", 200, chat_view(message("m1", 1, "from bob", sender=2)))

    open_chat.wait_for("from bob", seconds=POLL_SECONDS)


def test_own_message_changes_to_read_when_the_other_user_reads_it(server, open_chat):
    server.respond("POST", "/chats/c1/messages", 201, message("m1", 1, "hello bob"))
    open_chat.press("hello bob" + ENTER)
    open_chat.wait_for("sent")

    server.respond("GET", "/chats/c1", 200, chat_view(message("m1", 1, "hello bob", status="read")))

    open_chat.wait_for("read", seconds=POLL_SECONDS)
    assert "sent" not in open_chat.text()


def test_messages_are_shown_in_order(server, open_chat):
    server.respond("GET", "/chats/c1", 200, chat_view(
        message("m1", 1, "first message"),
        message("m2", 2, "second message", sender=2),
    ))

    open_chat.wait_for("second message", seconds=POLL_SECONDS)
    text = open_chat.text()
    assert text.index("first message") < text.index("second message")
