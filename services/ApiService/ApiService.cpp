#include "ApiService.h"

#include <httplib.h>

#include <nlohmann/json.hpp>
#include <optional>
#include <utility>

#include "../../api/json/JsonMapper.h"

using nlohmann::json;

namespace {
    constexpr int connectionTimeoutSeconds = 2;
    constexpr int readTimeoutSeconds = 5;
    constexpr auto jsonContentType = "application/json";
    constexpr auto unexpectedResponse = "Unexpected response from the server";

    httplib::Client makeClient(const std::string& serverUrl) {
        httplib::Client client(serverUrl);
        client.set_connection_timeout(connectionTimeoutSeconds);
        client.set_read_timeout(readTimeoutSeconds);
        return client;
    }

    httplib::Headers authHeaders(const std::string& token) {
        return {{"Authorization", "Bearer " + token}};
    }

    std::string dump(const json& body) {
        return body.dump(-1, ' ', false, json::error_handler_t::replace);
    }

    // Responds with nothing but an error unless the server answered with what was expected.
    template <typename T>
    Response<T> parse(const httplib::Result& result) {
        if (!result) {
            return {std::nullopt, "Cannot reach the server"};
        }

        const json body = json::parse(result->body, nullptr, false);
        if (result->status >= 400) {
            const bool hasMessage = body.is_object() && body.contains("error") && body["error"].is_string();
            return {std::nullopt, hasMessage ? body["error"].get<std::string>() : "Request failed"};
        }

        try {
            return {body.get<T>(), ""};
        } catch (const json::exception&) {
            return {std::nullopt, unexpectedResponse};
        }
    }
}

ApiService::ApiService(std::string serverUrl) : serverUrl(std::move(serverUrl)) {}

Response<AuthResult> ApiService::registerUser(const std::string& username, const std::string& password) {
    return authenticate("/auth/register", username, password);
}

Response<AuthResult> ApiService::login(const std::string& username, const std::string& password) {
    return authenticate("/auth/login", username, password);
}

Response<AuthResult> ApiService::authenticate(const std::string& path, const std::string& username, const std::string& password) {
    const json body = {{"username", username}, {"password", password}};

    httplib::Client client = makeClient(serverUrl);
    Response<AuthResult> response = parse<AuthResult>(client.Post(path, dump(body), jsonContentType));
    if (response.response_data.has_value()) {
        token = response.response_data->token;
    }
    return response;
}

void ApiService::signOut() {
    token.clear();
}

Response<std::vector<ChatSummary>> ApiService::getChats() const {
    httplib::Client client = makeClient(serverUrl);
    return parse<std::vector<ChatSummary>>(client.Get("/chats", authHeaders(token)));
}

Response<ChatView> ApiService::getChatView(const std::string& chatId) const {
    httplib::Client client = makeClient(serverUrl);
    return parse<ChatView>(client.Get("/chats/" + chatId, authHeaders(token)));
}

Response<std::string> ApiService::createChat(std::string username) const {
    const json body = {{"username", username}};

    httplib::Client client = makeClient(serverUrl);
    const httplib::Result result = client.Post("/chats", authHeaders(token), dump(body), jsonContentType);

    Response<json> chat = parse<json>(result);
    if (!chat.response_data.has_value()) {
        return {std::nullopt, chat.error};
    }

    const json& created = *chat.response_data;
    const bool hasId = created.is_object() && created.contains("id") && created["id"].is_string()
        && !created["id"].get_ref<const std::string&>().empty();
    if (!hasId) {
        return {std::nullopt, unexpectedResponse};
    }
    return {created["id"].get<std::string>(), ""};
}

Response<bool> ApiService::deleteChat(const std::string& chatId) const {
    httplib::Client client = makeClient(serverUrl);
    const httplib::Result result = client.Delete("/chats/" + chatId, authHeaders(token));

    // A success has no body, so there's nothing for parse to read.
    if (result && result->status == 204) {
        return {true, ""};
    }
    const Response<json> failure = parse<json>(result);
    return {std::nullopt, failure.error.empty() ? unexpectedResponse : failure.error};
}

Response<Message> ApiService::sendMessage(const std::string& chatId, const std::string& text) const {
    const json body = {{"text", text}};

    httplib::Client client = makeClient(serverUrl);
    return parse<Message>(client.Post("/chats/" + chatId + "/messages", authHeaders(token), dump(body), jsonContentType));
}
