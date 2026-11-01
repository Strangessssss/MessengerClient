#include <cstdlib>
#include <string>

#include "services/ApiService/ApiService.h"
#include "services/UiService/UiService.h"

namespace {
    // The server listens on IPv4 only, and "localhost" can resolve to IPv6 first and time out.
    constexpr auto defaultServerUrl = "http://127.0.0.1:8080";
}

int main() {
    const char* serverUrl = std::getenv("MESSENGER_SERVER_URL");

    ApiService apiService(serverUrl != nullptr ? std::string(serverUrl) : std::string(defaultServerUrl));
    UiService uiService(apiService);

    uiService.run();
    return 0;
}
