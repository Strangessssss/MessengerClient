# Messenger Client

The terminal client of a small CLI messenger. You register or log in, start a chat with another user by username, and
exchange messages. It needs a running [Messenger Server](https://github.com/Strangessssss/MessengerServer).

The server repository has the [scope and requirements](https://github.com/Strangessssss/MessengerServer/blob/main/docs/SCOPE.md)
the [roadmap](https://github.com/Strangessssss/MessengerServer/blob/main/docs/ROADMAP.md) and the
[essay](https://github.com/Strangessssss/MessengerServer/blob/main/docs/ESSAY.md), which cover the whole system. To work on
the client, see [CONTRIBUTING.md](CONTRIBUTING.md).

## Build

The target platform is Linux. You need a C++20 compiler (GCC 13 or newer), CMake 3.24 or newer and Git. On Ubuntu 24.04:

```bash
sudo apt install build-essential cmake ninja-build git
```

Libraries are downloaded by CMake on the first build, so that build needs internet access.

```bash
git clone https://github.com/Strangessssss/MessengerClient.git
cd MessengerClient
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

CMake also supports Windows 10 or newer with MinGW, such as CLion's bundled toolchain.

## Run

Start the server first, following [its README](https://github.com/Strangessssss/MessengerServer/blob/main/docs/README.md#run), and then:

```bash
./build/MessengerClient
```

It connects to `http://127.0.0.1:8080`. To use another server, set `MESSENGER_SERVER_URL`:

```bash
MESSENGER_SERVER_URL=http://192.168.1.20:9000 ./build/MessengerClient
```

## Use

To try a conversation, run the client in two terminals and register two different users.

1. **Sign in.** Enter a username and password. Choose **Register** the first time, with a password of at least 8
   characters, or **Log in** later. Enter in a field logs in.
2. **Start a chat.** Reach **New chat** by clicking it or pressing Down past the last chat. With no chats or one chat, Tab
   also works. Enter the other user's exact username and press Enter.
3. **Send a message.** Type it in the message field and press Enter.
4. **Switch chats.** Choose a chat on the left to open it, and press Enter to move to the message field. Each chat shows
   its latest message, and a number after the name, like `bob (2)`, shows how many messages you haven't read.
5. **Close a chat.** Press Ctrl+W to close the opened chat. It stays in your list.
6. **Delete a chat.** Press Ctrl+D twice while it's open. This deletes the chat and its messages **for both users**.
7. **Sign out.** Press Ctrl+O to return to the login page.

New messages appear within about 2 seconds. Your own messages show `sent`, and then `read` once the other user opens the chat.

| Key | Action |
|---|---|
| Up and Down, or mouse | Choose a chat, which opens it. Down past the last chat reaches **New chat** |
| Right | Move from the left side to the message field |
| Tab | In the chats list, cycle through the chats, or go to **New chat** if there's only one chat. In the other fields, move to the next field on the left side |
| Enter | Confirm the current field. In the chats list, move to the message field |
| Ctrl+W | Close the opened chat |
| Ctrl+D, twice | Delete the opened chat for both users |
| Ctrl+O | Sign out |
| Esc | Quit |

### Good to know

- The server keeps data in memory. After it restarts, register again, as all users and chats are gone.
- You sign in each time you start the client.
- "Missing or invalid token" means your sign in expired, after 24 hours, or the server restarted. Press Ctrl+O and sign in
  again.
- "Cannot reach the server" means the server isn't running or the URL is wrong. Your message stays in the field, so you can
  send it again, and the client keeps trying to refresh.
- Connections aren't encrypted, so use it only locally or on a trusted network.

## Known limitations

- Tab doesn't visit every field. With 2 or more chats it only cycles through them, and it never moves from the left side to
  the message field.
- Sign in isn't saved, so it's needed on each start.
- Unsent messages aren't saved or resent automatically. They stay in the message field until the program closes.
- Each poll fetches all messages of the open chat.
- An open chat counts as read, even if the terminal window is in the background.
- All connections use plain HTTP, and there's no HTTPS support.
