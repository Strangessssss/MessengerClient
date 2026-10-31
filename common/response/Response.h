#pragma once
#include <optional>
#include <string>

// Holds the data of a successful request, or the error to show when it failed.
template <typename T>
struct Response {
    std::optional<T> response_data;
    std::string error;
};
