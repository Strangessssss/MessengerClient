#pragma once
#include <nlohmann/json.hpp>

#include "../../entities/AuthResult.h"
#include "../../entities/ChatSummary.h"
#include "../../entities/ChatView.h"
#include "../../entities/Message.h"

inline void from_json(const nlohmann::json& json, AuthResult& auth) {
    json.at("userId").get_to(auth.userId);
    json.at("token").get_to(auth.token);
}

inline void from_json(const nlohmann::json& json, Message& message) {
    json.at("id").get_to(message.id);
    json.at("seq").get_to(message.seq);
    json.at("chatId").get_to(message.chatId);
    json.at("senderId").get_to(message.senderId);
    json.at("text").get_to(message.text);
    json.at("status").get_to(message.status);
    json.at("createdAt").get_to(message.createdAt);
}

inline void from_json(const nlohmann::json& json, ChatSummary& summary) {
    json.at("chatId").get_to(summary.chatId);
    json.at("partnerUsername").get_to(summary.partnerUsername);
    json.at("unreadCount").get_to(summary.unreadCount);

    const nlohmann::json& latest = json.at("latestMessage");
    if (!latest.is_null()) {
        summary.latestMessageText = latest.at("text").get<std::string>();
    }
}

inline void from_json(const nlohmann::json& json, ChatView& view) {
    json.at("chatId").get_to(view.chatId);
    json.at("partnerUsername").get_to(view.partnerUsername);
    json.at("messages").get_to(view.messages);
}
