#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "api/json/JsonMapper.h"

using nlohmann::json;

namespace {
    json messageJson() {
        return {
            {"id", "m1"},
            {"seq", 3},
            {"chatId", "c1"},
            {"senderId", 7},
            {"text", "hi"},
            {"status", "read"},
            // Past what fits in 32 bits, as every current time in milliseconds is.
            {"createdAt", 1700000000000LL},
        };
    }

    json summaryJson() {
        return {
            {"chatId", "c1"},
            {"partnerUsername", "bob"},
            {"unreadCount", 2},
            {"latestMessage", {{"text", "hi"}}},
        };
    }
}

TEST(JsonMapperTests, ReadsAuthResult) {
    const auto auth = json({{"userId", 7}, {"token", "abc"}}).get<AuthResult>();

    EXPECT_EQ(auth.userId, 7);
    EXPECT_EQ(auth.token, "abc");
}

TEST(JsonMapperTests, RejectsAuthResultWithoutToken) {
    EXPECT_THROW(json({{"userId", 7}}).get<AuthResult>(), json::exception);
}

TEST(JsonMapperTests, ReadsMessage) {
    const auto message = messageJson().get<Message>();

    EXPECT_EQ(message.id, "m1");
    EXPECT_EQ(message.seq, 3);
    EXPECT_EQ(message.chatId, "c1");
    EXPECT_EQ(message.senderId, 7);
    EXPECT_EQ(message.text, "hi");
    EXPECT_EQ(message.status, "read");
    EXPECT_EQ(message.createdAt, 1700000000000LL);
}

TEST(JsonMapperTests, RejectsMessageMissingAnyField) {
    for (const char* field : {"id", "seq", "chatId", "senderId", "text", "status", "createdAt"}) {
        SCOPED_TRACE(field);
        json message = messageJson();
        message.erase(field);

        EXPECT_THROW(message.get<Message>(), json::exception);
    }
}

TEST(JsonMapperTests, RejectsMessageWithAWrongType) {
    json message = messageJson();
    message["seq"] = "three";

    EXPECT_THROW(message.get<Message>(), json::exception);
}

TEST(JsonMapperTests, ReadsChatSummary) {
    const auto summary = summaryJson().get<ChatSummary>();

    EXPECT_EQ(summary.chatId, "c1");
    EXPECT_EQ(summary.partnerUsername, "bob");
    EXPECT_EQ(summary.unreadCount, 2);
    EXPECT_EQ(summary.latestMessageText, "hi");
}

TEST(JsonMapperTests, ReadsChatSummaryWithNoMessages) {
    json summary = summaryJson();
    summary["latestMessage"] = nullptr;

    EXPECT_EQ(summary.get<ChatSummary>().latestMessageText, std::nullopt);
}

TEST(JsonMapperTests, RejectsChatSummaryWithoutLatestMessage) {
    json summary = summaryJson();
    summary.erase("latestMessage");

    EXPECT_THROW(summary.get<ChatSummary>(), json::exception);
}

TEST(JsonMapperTests, RejectsChatSummaryWithLatestMessageWithoutText) {
    json summary = summaryJson();
    summary["latestMessage"] = json::object();

    EXPECT_THROW(summary.get<ChatSummary>(), json::exception);
}

TEST(JsonMapperTests, ReadsChatViewWithMessagesInOrder) {
    json second = messageJson();
    second["id"] = "m2";

    const auto view = json({
        {"chatId", "c1"},
        {"partnerUsername", "bob"},
        {"messages", json::array({messageJson(), second})},
    }).get<ChatView>();

    EXPECT_EQ(view.chatId, "c1");
    EXPECT_EQ(view.partnerUsername, "bob");
    ASSERT_EQ(view.messages.size(), 2);
    EXPECT_EQ(view.messages[0].id, "m1");
    EXPECT_EQ(view.messages[1].id, "m2");
}

TEST(JsonMapperTests, ReadsChatViewWithNoMessages) {
    const auto view = json({
        {"chatId", "c1"},
        {"partnerUsername", "bob"},
        {"messages", json::array()},
    }).get<ChatView>();

    EXPECT_TRUE(view.messages.empty());
}

TEST(JsonMapperTests, RejectsChatViewWithAMalformedMessage) {
    const json view = {
        {"chatId", "c1"},
        {"partnerUsername", "bob"},
        {"messages", json::array({json::object()})},
    };

    EXPECT_THROW(view.get<ChatView>(), json::exception);
}
