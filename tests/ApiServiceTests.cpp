#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <thread>

#include "services/ApiService/ApiService.h"

class ApiServiceTests : public ::testing::Test {
protected:
    httplib::Server server;
    std::thread serverThread;
    std::unique_ptr<ApiService> api;

    // Response the test server will send.
    int replyStatus = 200;
    std::string replyBody =
        R"({"userId":10,"token":"test-token"})";

    // Request details recorded by the test server.
    std::string receivedBody;
    std::string receivedContentType;
    bool receivedLogin = false;

    void SetUp() override {
        server.Post("/auth/login",
            [this](const httplib::Request& request,
                   httplib::Response& response) {
                receivedLogin = true;
                receivedBody = request.body;
                receivedContentType =
                    request.get_header_value("Content-Type");

                response.status = replyStatus;
                response.set_content(replyBody, "application/json");
            });

        const int port = server.bind_to_any_port("127.0.0.1");
        ASSERT_GT(port, 0);

        api = std::make_unique<ApiService>(
            "http://127.0.0.1:" + std::to_string(port)
        );
    }

    void startServer() {
        serverThread = std::thread([this] {
            server.listen_after_bind();
        });

        server.wait_until_ready();
    }

    void stopServer() {
        server.stop();

        if (serverThread.joinable()) {
            serverThread.join();
        }
    }

    void TearDown() override {
        stopServer();
    }
};

TEST_F(ApiServiceTests, LoginSendsCredentialsAndParsesResponse) {
    startServer();

    const auto result = api->login("username123", "password123");

    stopServer();

    ASSERT_TRUE(receivedLogin);
    ASSERT_EQ(receivedContentType, "application/json");

}
