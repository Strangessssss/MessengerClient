#pragma once
#include <string>
#include <vector>

#include "../../common/response/Response.h"
#include "../../entities/AuthResult.h"
#include "../../entities/ChatSummary.h"
#include "../../entities/ChatView.h"
#include "../../entities/Message.h"

// Talks to the server. Once logged in, its requests are safe to make from several threads.
class ApiService {
    public:
        explicit ApiService(std::string serverUrl);

        // Both keep the token of a successful response for the later requests.
        Response<AuthResult> registerUser(const std::string& username, const std::string& password);
        Response<AuthResult> login(const std::string& username, const std::string& password);

        Response<std::vector<ChatSummary>> getChats() const;

        // Marks the chat's messages as read.
        Response<ChatView> getChatView(const std::string& chatId) const;

        // Responds with the ID of the new chat, or of the existing chat with that user.
        Response<std::string> createChat(std::string username) const;

        Response<Message> sendMessage(const std::string& chatId, const std::string& text) const;

    private:
        Response<AuthResult> authenticate(const std::string& path, const std::string& username, const std::string& password);

        const std::string serverUrl;
        std::string token;
};
