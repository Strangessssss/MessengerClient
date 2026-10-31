#pragma once
#include <string>
#include <vector>

#include "Message.h"

struct ChatView {
    std::string chatId;
    std::string partnerUsername;
    std::vector<Message> messages;
};
