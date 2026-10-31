#pragma once
#include <optional>
#include <string>

struct ChatSummary {
    std::string chatId;
    std::string partnerUsername;
    std::optional<std::string> latestMessageText;
};
