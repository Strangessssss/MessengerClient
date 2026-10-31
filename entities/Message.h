#pragma once
#include <cstdint>
#include <string>

struct Message {
    std::string id; // GUID
    int seq = 0;

    std::string chatId;

    int senderId = 0;

    std::string text;

    std::string status;

    std::int64_t createdAt = 0; // Milliseconds since the epoch
};
