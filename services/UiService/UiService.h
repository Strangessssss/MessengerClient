#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "../../entities/AuthResult.h"
#include "../../entities/ChatSummary.h"
#include "../../entities/ChatView.h"
#include "../ApiService/ApiService.h"

// Draws the terminal UI. All members are used on the UI thread, except that the
// polling thread reads polledChatId and fetches updates from the ApiService.
class UiService {
    public:
        explicit UiService(ApiService& apiService);

        // Blocks until the user quits.
        void run();

    private:
        ftxui::Component buildLoginScreen();
        ftxui::Component buildChatsScreen();
        ftxui::Element renderOpenChat() const;

        void authenticate(const Response<AuthResult>& response);
        void openChat(const std::string& chatId);
        void createChat();
        void sendMessage();

        // Returns to the chats list. The chat stays on the server.
        void closeChat();

        // Asks for confirmation the first time, and deletes the opened chat the second time.
        void deleteOpenedChat();

        // Stops polling, forgets this user's data and returns to the login page.
        void signOut();

        void clearOpenedChat();
        void showChats(const Response<std::vector<ChatSummary>>& response);
        void pollLoop(std::stop_token stopToken);

        ApiService& apiService;
        ftxui::ScreenInteractive screen;

        int activeScreen = 0;
        ftxui::Component usernameInput;
        ftxui::Component chatMenu;
        ftxui::Component messageInput;

        std::string usernameText;
        std::string passwordText;
        std::string newChatText;
        std::string messageText;
        std::string loginError;

        // An error from the user's last action, and one from the last poll.
        std::string actionError;
        std::string pollError;

        std::string username;
        int userId = 0;

        std::vector<ChatSummary> chats;
        std::vector<std::string> chatLabels;
        int selectedChat = 0;
        std::optional<ChatView> openedChat;
        bool confirmingDelete = false;

        std::mutex pollMutex;
        std::condition_variable_any pollWait;
        std::string polledChatId;

        // Declared last so it stops before the members it uses are destroyed.
        std::jthread poller;
};
