# Contributing

## Setup

Follow [Build in the README](README.md#build). The first configure downloads the libraries.

## Structure

```
main.cpp              Reads MESSENGER_SERVER_URL and starts the UI
services/ApiService/  All HTTP: sends requests, keeps the token, turns responses into Response<T>
services/UiService/   All screens (FTXUI), user actions and the poller thread
api/json/             Converts server JSON to entities
entities/             Plain data: AuthResult, ChatSummary, ChatView, Message
common/response/      Response<T>: data, or the error to show
tests/                GoogleTest unit tests, with a fake HTTP server in tests/support/
tests/module/         pytest module tests, which run the real binary in a pseudo-terminal
```

## Design notes

- **Only the UI thread touches UI state.** The poller makes its requests, then hands the results to the UI thread with
  `screen.Post`. It never changes UI state itself.
- **`pollMutex` protects `polledChatId`**, the only value the two threads share directly. Always copy it and release the
  lock before a network request, or a slow server freezes the UI.
- **Poll results can be stale.** The user may switch chats or send a message while a request is running, so `pollLoop`
  checks the result still applies before using it.
- **`poller` is declared last in `UiService`**, so it stops before the members it uses are destroyed. Member order matters here.
- **Network errors are values, not exceptions.** `ApiService` always returns a `Response<T>`, and the UI shows `error`.
- **`MessengerClientCore` is everything except `main.cpp`**, so tests link exactly the code the client runs.

## Tests

Unit tests use GoogleTest and cover `ApiService`, against a local fake HTTP server, and `JsonMapper`. Build and run them with:

```
cmake --build <build dir> --target MessengerClientTests
ctest --test-dir <build dir> --output-on-failure
```

Module tests use pytest and live in `tests/module/`. They start the real client in a pseudo-terminal, read its screen with
the `pyte` terminal emulator, and connect it to a fake server in `conftest.py`, so a test decides exactly what the server
answers. They cover the login page, chats and messages. They look for the binary in `build/` and `cmake-build-*/`, or use
`MESSENGER_CLIENT_BIN`. A test marked `xfail` describes a known limitation, and fails the run once that limitation is
fixed, so remove the mark then. Build the client first, and then:

```bash
python3 -m venv .venv
.venv/bin/pip install -r tests/module/requirements.txt
.venv/bin/python -m pytest tests/module -c tests/module/pytest.ini
```

On Windows, use `.venv\Scripts\` instead of `.venv/bin/`, with MinGW's `bin` folder on `PATH`. The tests have only been run
on Windows so far.

Nothing runs the real client and the real server together, so also check a change to `UiService` by hand against a running
[server](https://github.com/Strangessssss/MessengerServer), with two clients if it affects a conversation.

Planned work for the client is in the [roadmap](https://github.com/Strangessssss/MessengerServer/blob/main/docs/ROADMAP.md).

## Conventions

- C++20, with 4 spaces of indentation. Types are `PascalCase`, functions and variables are `camelCase`.
- Comments explain why, or a contract that a signature can't show.
- Add new source files to `MessengerClientCore` in `CMakeLists.txt`, and new test files to `MessengerClientTests`.

## Pull requests

1. Branch from `main`, and keep one change per pull request.
2. Build on Linux, run the tests and check the change by hand.
3. Update the README when behavior changes.
4. Changes that depend on an API change should link the matching [server](https://github.com/Strangessssss/MessengerServer) pull request.
