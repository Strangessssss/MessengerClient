cmake_minimum_required(VERSION 4.3)
project(MessengerClient)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v7.0.3
)

# The server speaks plain HTTP, so no TLS or compression backends are needed.
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.64.0
)
FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(ftxui httplib json)

add_executable(MessengerClient main.cpp
        api/json/JsonMapper.h
        common/response/Response.h
        entities/AuthResult.h
        entities/ChatSummary.h
        entities/ChatView.h
        entities/Message.h
        services/ApiService/ApiService.cpp
        services/ApiService/ApiService.h
        services/UiService/UiService.cpp
        services/UiService/UiService.h
)

target_link_libraries(MessengerClient PRIVATE
        ftxui::component ftxui::dom ftxui::screen
        httplib::httplib
        nlohmann_json::nlohmann_json
)
if(WIN32)
    # cpp-httplib needs Windows 10, and MinGW targets an older version by default.
    target_compile_definitions(MessengerClient PRIVATE _WIN32_WINNT=0x0A00)
endif()
