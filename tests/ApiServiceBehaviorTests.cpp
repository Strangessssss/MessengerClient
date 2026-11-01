#include <gtest/gtest.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "services/ApiService/ApiService.h"
#include "tests/support/FakeServer.h"

using nlohmann::json;

namespace {
    const std::string authBody = R"({"userId":7,"token":"secret-token"})";
    const std::string messageBody =
        R"({"id":"m1","seq":1,"chatId":"c1","senderId":7,"text":"hi","status":"sent","createdAt":1000})";

    class ApiServiceBehaviorTests : public testing::Test {
        protected:
            void logIn() {
                server.respondWith(200, authBody);
                ASSERT_TRUE(api.login("alice", "password").response_data.has_value());
            }

            FakeServer server;
            ApiService api{server.url()};
    };

    // Bodies that are not what createChat expects, even though their status says success.
    class ApiServiceCreateChatBadBodyTests : public ApiServiceBehaviorTests,
                                             public testing::WithParamInterface<std::string> {};

    template <typename T>
    void expectFailure(const Response<T>& response, const std::string& error) {
        EXPECT_FALSE(response.response_data.has_value());
        EXPECT_EQ(response.error, error);
    }

    const char* const unexpectedResponse = "Unexpected response from the server";
}

// Authentication

TEST_F(ApiServiceBehaviorTests, LoginPostsTheCredentials) {
    server.respondWith(200, authBody);

    api.login("alice", "password");

    const FakeServer::Request request = server.lastRequest();
    EXPECT_EQ(request.method, "POST");
    EXPECT_EQ(request.path, "/auth/login");
    EXPECT_EQ(request.contentType, "application/json");
    EXPECT_EQ(json::parse(request.body), json({{"username", "alice"}, {"password", "password"}}));
}

TEST_F(ApiServiceBehaviorTests, RegisterPostsTheCredentials) {
    server.respondWith(201, authBody);

    api.registerUser("alice", "password");

    const FakeServer::Request request = server.lastRequest();
    EXPECT_EQ(request.method, "POST");
    EXPECT_EQ(request.path, "/auth/register");
    EXPECT_EQ(json::parse(request.body), json({{"username", "alice"}, {"password", "password"}}));
}

TEST_F(ApiServiceBehaviorTests, LoginRespondsWithTheUserAndToken) {
    server.respondWith(200, authBody);

    const Response<AuthResult> response = api.login("alice", "password");

    ASSERT_TRUE(response.response_data.has_value());
    EXPECT_EQ(response.response_data->userId, 7);
    EXPECT_EQ(response.response_data->token, "secret-token");
    EXPECT_EQ(response.error, "");
}

TEST_F(ApiServiceBehaviorTests, AuthenticationRequestsSendNoToken) {
    logIn();

    api.login("alice", "password");

    EXPECT_EQ(server.lastRequest().authorization, "");
}

TEST_F(ApiServiceBehaviorTests, LaterRequestsSendTheTokenFromLogin) {
    logIn();
    server.respondWith(200, "[]");

    api.getChats();

    EXPECT_EQ(server.lastRequest().authorization, "Bearer secret-token");
}

TEST_F(ApiServiceBehaviorTests, LaterRequestsSendTheTokenFromRegister) {
    server.respondWith(201, authBody);
    api.registerUser("alice", "password");
    server.respondWith(200, "[]");

    api.getChats();

    EXPECT_EQ(server.lastRequest().authorization, "Bearer secret-token");
}

TEST_F(ApiServiceBehaviorTests, FailedLoginKeepsThePreviousToken) {
    logIn();
    server.respondWith(401, R"({"error":"Invalid username or password"})");
    api.login("alice", "wrong");
    server.respondWith(200, "[]");

    api.getChats();

    EXPECT_EQ(server.lastRequest().authorization, "Bearer secret-token");
}

TEST_F(ApiServiceBehaviorTests, SignOutForgetsTheToken) {
    logIn();
    api.signOut();
    server.respondWith(200, "[]");
    const size_t requestsBefore = server.requests().size();

    api.getChats();

    const std::vector<FakeServer::Request> requests = server.requests();
    for (size_t index = requestsBefore; index < requests.size(); ++index) {
        EXPECT_EQ(requests[index].authorization.find("secret-token"), std::string::npos);
    }
}

TEST_F(ApiServiceBehaviorTests, LoginWithAnIncompleteResponseKeepsNoToken) {
    server.respondWith(200, R"({"userId":7})");

    expectFailure(api.login("alice", "password"), unexpectedResponse);
}

// Chats

TEST_F(ApiServiceBehaviorTests, GetChatsRequestsTheList) {
    logIn();
    server.respondWith(200, R"([
        {"chatId":"c1","partnerUsername":"bob","unreadCount":2,"latestMessage":{"text":"hi"}},
        {"chatId":"c2","partnerUsername":"eve","unreadCount":0,"latestMessage":null}
    ])");

    const auto response = api.getChats();

    const FakeServer::Request request = server.lastRequest();
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.path, "/chats");
    ASSERT_TRUE(response.response_data.has_value());
    ASSERT_EQ(response.response_data->size(), 2);
    EXPECT_EQ(response.response_data->at(0).chatId, "c1");
    EXPECT_EQ(response.response_data->at(0).latestMessageText, "hi");
    EXPECT_EQ(response.response_data->at(1).latestMessageText, std::nullopt);
}

TEST_F(ApiServiceBehaviorTests, GetChatViewRequestsTheChat) {
    logIn();
    server.respondWith(200, R"({"chatId":"c1","partnerUsername":"bob","messages":[)" + messageBody + "]}");

    const Response<ChatView> response = api.getChatView("c1");

    const FakeServer::Request request = server.lastRequest();
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.path, "/chats/c1");
    EXPECT_EQ(request.authorization, "Bearer secret-token");
    ASSERT_TRUE(response.response_data.has_value());
    EXPECT_EQ(response.response_data->partnerUsername, "bob");
    ASSERT_EQ(response.response_data->messages.size(), 1);
    EXPECT_EQ(response.response_data->messages[0].text, "hi");
}

TEST_F(ApiServiceBehaviorTests, CreateChatPostsTheUsernameAndRespondsWithTheId) {
    logIn();
    server.respondWith(201, R"({"id":"c1"})");

    const Response<std::string> response = api.createChat("bob");

    const FakeServer::Request request = server.lastRequest();
    EXPECT_EQ(request.method, "POST");
    EXPECT_EQ(request.path, "/chats");
    EXPECT_EQ(request.authorization, "Bearer secret-token");
    EXPECT_EQ(request.contentType, "application/json");
    EXPECT_EQ(json::parse(request.body), json({{"username", "bob"}}));
    EXPECT_EQ(response.response_data, "c1");
    EXPECT_EQ(response.error, "");
}

TEST_F(ApiServiceBehaviorTests, GetChatViewRespondsWithTheServerError) {
    logIn();
    server.respondWith(404, R"({"error":"Chat not found"})");

    expectFailure(api.getChatView("missing"), "Chat not found");
}

TEST_F(ApiServiceBehaviorTests, GetChatViewWithAMalformedMessageRespondsWithAnError) {
    logIn();
    server.respondWith(200, R"({"chatId":"c1","partnerUsername":"bob","messages":[{}]})");

    expectFailure(api.getChatView("c1"), unexpectedResponse);
}

TEST_F(ApiServiceBehaviorTests, CreateChatReadsTheRealServerResponse) {
    logIn();
    server.respondWith(200, R"({"id":"c1","userIds":[1,2],"createdAt":1700000000000})");

    const Response<std::string> response = api.createChat("bob");

    EXPECT_EQ(response.response_data, "c1");
    EXPECT_EQ(response.error, "");
}

TEST_F(ApiServiceBehaviorTests, GetChatsReadsTheRealServerResponse) {
    logIn();
    server.respondWith(200, R"([{"chatId":"c1","partnerUsername":"bob","unreadCount":0,
        "latestMessage":{"text":"hi","createdAt":1700000000000,"status":"read"}}])");

    const auto response = api.getChats();

    ASSERT_TRUE(response.response_data.has_value());
    ASSERT_EQ(response.response_data->size(), 1);
    EXPECT_EQ(response.response_data->at(0).latestMessageText, "hi");
}

TEST_F(ApiServiceBehaviorTests, CreateChatRespondsWithTheServerError) {
    logIn();
    server.respondWith(404, R"({"error":"User not found"})");

    expectFailure(api.createChat("nobody"), "User not found");
}

TEST_P(ApiServiceCreateChatBadBodyTests, RespondsWithAnError) {
    logIn();
    server.respondWith(200, GetParam());

    expectFailure(api.createChat("bob"), unexpectedResponse);
}

INSTANTIATE_TEST_SUITE_P(BadBodies, ApiServiceCreateChatBadBodyTests,
                         testing::Values("not json", "", "[]", "{}", R"({"id":5})", R"({"id":null})"));

TEST_F(ApiServiceBehaviorTests, DeleteChatSendsTheRequest) {
    logIn();
    server.respondWith(204, "");

    const Response<bool> response = api.deleteChat("c1");

    const FakeServer::Request request = server.lastRequest();
    EXPECT_EQ(request.method, "DELETE");
    EXPECT_EQ(request.path, "/chats/c1");
    EXPECT_EQ(request.authorization, "Bearer secret-token");
    EXPECT_EQ(response.response_data, true);
    EXPECT_EQ(response.error, "");
}

TEST_F(ApiServiceBehaviorTests, DeleteChatRespondsWithTheServerError) {
    logIn();
    server.respondWith(403, R"({"error":"Not your chat"})");

    expectFailure(api.deleteChat("c1"), "Not your chat");
}

TEST_F(ApiServiceBehaviorTests, DeleteChatTreatsAnyOtherSuccessAsUnexpected) {
    logIn();
    server.respondWith(200, "{}");

    expectFailure(api.deleteChat("c1"), unexpectedResponse);
}

// Messages

TEST_F(ApiServiceBehaviorTests, SendMessagePostsTheTextAndRespondsWithTheMessage) {
    logIn();
    server.respondWith(201, messageBody);

    const Response<Message> response = api.sendMessage("c1", "hi");

    const FakeServer::Request request = server.lastRequest();
    EXPECT_EQ(request.method, "POST");
    EXPECT_EQ(request.path, "/chats/c1/messages");
    EXPECT_EQ(request.authorization, "Bearer secret-token");
    EXPECT_EQ(request.contentType, "application/json");
    EXPECT_EQ(json::parse(request.body), json({{"text", "hi"}}));
    ASSERT_TRUE(response.response_data.has_value());
    EXPECT_EQ(response.response_data->id, "m1");
}

TEST_F(ApiServiceBehaviorTests, SendMessageRespondsWithTheServerError) {
    logIn();
    server.respondWith(400, R"({"error":"Message was rejected"})");

    expectFailure(api.sendMessage("c1", ""), "Message was rejected");
}

TEST_F(ApiServiceBehaviorTests, SendMessageWithAnIncompleteResponseRespondsWithAnError) {
    logIn();
    server.respondWith(201, R"({"id":"m1"})");

    expectFailure(api.sendMessage("c1", "hi"), unexpectedResponse);
}

TEST_F(ApiServiceBehaviorTests, SendMessageReplacesInvalidUtf8InsteadOfThrowing) {
    logIn();
    server.respondWith(201, messageBody);

    const Response<Message> response = api.sendMessage("c1", "bad\xFF");

    EXPECT_TRUE(response.response_data.has_value());
    EXPECT_EQ(json::parse(server.lastRequest().body).at("text"), "bad\xEF\xBF\xBD");
}

// Failures shared by every request

TEST_F(ApiServiceBehaviorTests, AnErrorStatusRespondsWithTheServerMessage) {
    server.respondWith(400, R"({"error":"Username is taken"})");

    expectFailure(api.registerUser("alice", "password"), "Username is taken");
}

TEST_F(ApiServiceBehaviorTests, AnErrorStatusWithoutAMessageRespondsWithAGenericError) {
    logIn();
    for (const std::string body : {"", "<html>Bad gateway</html>", "[]", "{}", R"({"error":5})"}) {
        SCOPED_TRACE(body);
        server.respondWith(500, body);

        expectFailure(api.getChats(), "Request failed");
    }
}

TEST_F(ApiServiceBehaviorTests, ASuccessWithTheWrongShapeRespondsWithAnError) {
    logIn();
    for (const std::string body : {"", "not json", "{}", R"([{"chatId":"c1"}])", "null"}) {
        SCOPED_TRACE(body);
        server.respondWith(200, body);

        expectFailure(api.getChats(), unexpectedResponse);
    }
}

TEST(ApiServiceUnreachableTests, RequestsRespondWithAnError) {
    // Takes a port that was free a moment ago, and that nothing listens on anymore.
    auto server = std::make_unique<FakeServer>();
    ApiService api(server->url());
    server->respondWith(200, authBody);
    ASSERT_TRUE(api.login("alice", "password").response_data.has_value());
    server.reset();

    // Each failed connection can take the whole timeout, so this covers each way of handling a result once.
    const std::string error = "Cannot reach the server";
    expectFailure(api.login("alice", "password"), error);
    expectFailure(api.createChat("bob"), error);
    expectFailure(api.deleteChat("c1"), error);
}
