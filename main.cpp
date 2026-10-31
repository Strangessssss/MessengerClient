#include "services/ApiService/ApiService.h"
#include "services/UiService/UiService.h"

namespace {
    // The server listens on IPv4 only, and "localhost" can resolve to IPv6 first and time out.
    constexpr auto serverUrl = "http://127.0.0.1:8080";
}

int main() {
    ApiService apiService(serverUrl);
    UiService uiService(apiService);

    uiService.run();
    return 0;
}
