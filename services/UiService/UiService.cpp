#include "UiService.h"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <charconv>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

using namespace ftxui;

namespace {
    constexpr auto pollInterval = std::chrono::seconds(2);
    constexpr int sidebarWidth = 32;
    constexpr int loginWidth = 44;

    std::string formatTime(std::int64_t milliseconds) {
        const std::time_t seconds = milliseconds / 1000;
        std::ostringstream stream;
        stream << std::put_time(std::localtime(&seconds), "%H:%M");
        return stream.str();
    }

    Component singleLineInput(std::string* text, const std::string& placeholder, std::function<void()> onEnter, bool password = false) {
        InputOption option;
        option.multiline = false;
        option.password = password;
        option.on_enter = std::move(onEnter);
        return Input(text, placeholder, option);
    }
}

UiService::UiService(ApiService& apiService)
    : apiService(apiService), screen(ScreenInteractive::Fullscreen()) {}

void UiService::run() {
    Component screens = Container::Tab({buildLoginScreen(), buildChatsScreen()}, &activeScreen);
    Component root = CatchEvent(screens, [this](const Event& event) {
        if (event == Event::Escape) {
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(root);

    // The poller posts to the screen, so it has to finish while the screen is alive.
    poller = std::jthread();
}

Component UiService::buildLoginScreen() {
    const auto submitLogin = [this] { authenticate(apiService.login(usernameText, passwordText)); };
    const auto submitRegister = [this] { authenticate(apiService.registerUser(usernameText, passwordText)); };

    Component usernameInput = singleLineInput(&usernameText, "username", submitLogin);
    Component passwordInput = singleLineInput(&passwordText, "password", submitLogin, true);
    Component loginButton = Button("Log in", submitLogin, ButtonOption::Ascii());
    Component registerButton = Button("Register", submitRegister, ButtonOption::Ascii());

    Component form = Container::Vertical({
        usernameInput,
        passwordInput,
        Container::Horizontal({loginButton, registerButton}),
    });

    return Renderer(form, [=, this] {
        return vbox({
            text("Messenger") | bold | center,
            separator(),
            hbox({text("Username: "), usernameInput->Render()}),
            hbox({text("Password: "), passwordInput->Render()}),
            separator(),
            hbox({loginButton->Render(), text("  "), registerButton->Render()}) | center,
            text(loginError) | color(Color::Red),
        }) | border | size(WIDTH, EQUAL, loginWidth) | center;
    });
}

Component UiService::buildChatsScreen() {
    MenuOption menuOption = MenuOption::Vertical();
    menuOption.on_change = [this] {
        if (selectedChat < static_cast<int>(chats.size())) {
            openChat(chats[selectedChat].chatId);
        }
    };
    menuOption.on_enter = [this] {
        if (selectedChat < static_cast<int>(chats.size())) {
            openChat(chats[selectedChat].chatId);
            messageInput->TakeFocus();
        }
    };

    Component chatMenu = Menu(&chatLabels, &selectedChat, menuOption);
    Component newChatInput = singleLineInput(&newChatText, "username", [this] { createChat(); });
    messageInput = singleLineInput(&messageText, "Type a message and press Enter", [this] { sendMessage(); });

    Component layout = Container::Horizontal({
        Container::Vertical({chatMenu, newChatInput}),
        messageInput,
    });

    return Renderer(layout, [=, this] {
        const std::string& error = actionError.empty() ? pollError : actionError;

        return vbox({
            hbox({
                text(username) | bold,
                filler(),
                text("Tab: switch field  Esc: quit") | dim,
            }),
            separator(),
            hbox({
                vbox({
                    text("Chats") | bold,
                    chatMenu->Render() | vscroll_indicator | yframe | flex,
                    separator(),
                    hbox({text("New chat: "), newChatInput->Render()}),
                }) | size(WIDTH, EQUAL, sidebarWidth),
                separator(),
                vbox({
                    renderOpenChat() | flex,
                    separator(),
                    messageInput->Render(),
                }) | flex,
            }) | flex,
            text(error) | color(Color::Red),
        }) | border;
    });
}

Element UiService::renderOpenChat() const {
    if (!openedChat.has_value()) {
        return text("Select a chat, or start one with a username") | dim | center;
    }

    Elements rows;
    for (const Message& message : openedChat->messages) {
        const bool isMine = message.senderId == userId;

        std::string details = "  " + formatTime(message.createdAt);
        if (isMine) {
            details += "  " + message.status;
        }

        rows.push_back(hbox({
            text(isMine ? "You" : openedChat->partnerUsername) | bold | color(isMine ? Color::Green : Color::Cyan),
            text(details) | dim,
        }));
        rows.push_back(paragraph(message.text));
    }

    return vbox({
        text(openedChat->partnerUsername) | bold,
        separator(),
        // Keeps the newest messages in view.
        vbox(std::move(rows)) | focusPositionRelative(0, 1) | vscroll_indicator | yframe | flex,
    });
}

void UiService::authenticate(const Response<AuthResult>& response) {
    if (!response.response_data.has_value()) {
        loginError = response.error;
        return;
    }

    username = usernameText;
    userId = response.response_data->userId;
    passwordText.clear();
    loginError.clear();
    activeScreen = 1;

    showChats(apiService.getChats());
    poller = std::jthread([this](std::stop_token stopToken) { pollLoop(stopToken); });
}

void UiService::openChat(const std::string& chatId) {
    Response<ChatView> response = apiService.getChatView(chatId);
    actionError = response.error;
    if (!response.response_data.has_value()) {
        return;
    }

    openedChat = std::move(response.response_data);

    const std::scoped_lock lock(pollMutex);
    polledChatId = chatId;
}

void UiService::createChat() {
    const std::optional otherUserUsername = newChatText;
    if (!otherUserUsername.has_value()) {
        actionError = "Enter the username of a user";
        return;
    }

    const Response<std::string> response = apiService.createChat(*otherUserUsername);
    actionError = response.error;
    if (!response.response_data.has_value()) {
        return;
    }

    newChatText.clear();
    openChat(*response.response_data);
    showChats(apiService.getChats());
    messageInput->TakeFocus();
}

void UiService::sendMessage() {
    if (!openedChat.has_value() || messageText.empty()) {
        return;
    }

    const Response<Message> response = apiService.sendMessage(openedChat->chatId, messageText);
    actionError = response.error;
    if (!response.response_data.has_value()) {
        return;
    }

    messageText.clear();
    openedChat->messages.push_back(*response.response_data);
    showChats(apiService.getChats());
}

void UiService::showChats(const Response<std::vector<ChatSummary>>& response) {
    pollError = response.error;
    if (!response.response_data.has_value()) {
        return;
    }

    chats = *response.response_data;
    chatLabels.clear();

    bool openedChatExists = false;
    for (int index = 0; index < static_cast<int>(chats.size()); ++index) {
        const ChatSummary& chat = chats[index];
        chatLabels.push_back(chat.partnerUsername + ": " + chat.latestMessageText.value_or("(no messages)"));

        // The list can change order, so the selection follows the opened chat.
        if (openedChat.has_value() && chat.chatId == openedChat->chatId) {
            selectedChat = index;
            openedChatExists = true;
        }
    }

    // The other user may have deleted the chat.
    if (openedChat.has_value() && !openedChatExists) {
        openedChat.reset();

        const std::scoped_lock lock(pollMutex);
        polledChatId.clear();
    }
}

void UiService::pollLoop(std::stop_token stopToken) {
    while (true) {
        std::string chatId;
        {
            std::unique_lock lock(pollMutex);
            pollWait.wait_for(lock, stopToken, pollInterval, [] { return false; });
            chatId = polledChatId;
        }
        if (stopToken.stop_requested()) {
            return;
        }

        Response<std::vector<ChatSummary>> chatsResponse = apiService.getChats();
        std::optional<Response<ChatView>> viewResponse;
        if (!chatId.empty()) {
            viewResponse = apiService.getChatView(chatId);
        }

        screen.Post([this, chatsResponse = std::move(chatsResponse), viewResponse = std::move(viewResponse)] {
            // This update may be older than a chat switch or a sent message made meanwhile.
            if (viewResponse.has_value() && viewResponse->response_data.has_value() && openedChat.has_value()) {
                const ChatView& view = *viewResponse->response_data;
                if (view.chatId == openedChat->chatId && view.messages.size() >= openedChat->messages.size()) {
                    openedChat = view;
                }
            }
            showChats(chatsResponse);
        });
        screen.PostEvent(Event::Custom);
    }
}
