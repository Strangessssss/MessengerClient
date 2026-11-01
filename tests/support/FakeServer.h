#pragma once
#include <httplib.h>

#include <mutex>
#include <string>
#include <thread>
#include <vector>

// A local HTTP server that gives one canned response to everything and records what it was asked.
class FakeServer {
    public:
        struct Request {
            std::string method;
            std::string path;
            std::string body;
            std::string authorization;
            std::string contentType;
        };

        FakeServer() {
            const auto handler = [this](const httplib::Request& request, httplib::Response& response) {
                handle(request, response);
            };
            server.Get(".*", handler);
            server.Post(".*", handler);
            server.Delete(".*", handler);

            port = server.bind_to_any_port("127.0.0.1");
            listener = std::thread([this] { server.listen_after_bind(); });
            server.wait_until_ready();
        }

        ~FakeServer() {
            server.stop();
            listener.join();
        }

        FakeServer(const FakeServer&) = delete;
        FakeServer& operator=(const FakeServer&) = delete;

        std::string url() const {
            return "http://127.0.0.1:" + std::to_string(port);
        }

        void respondWith(int status, const std::string& body) {
            const std::scoped_lock lock(mutex);
            responseStatus = status;
            responseBody = body;
        }

        std::vector<Request> requests() const {
            const std::scoped_lock lock(mutex);
            return recorded;
        }

        // The most recent request. Fails loudly in a test that expected one and got none.
        Request lastRequest() const {
            const std::scoped_lock lock(mutex);
            return recorded.at(recorded.size() - 1);
        }

    private:
        void handle(const httplib::Request& request, httplib::Response& response) {
            const std::scoped_lock lock(mutex);
            recorded.push_back({
                request.method,
                request.path,
                request.body,
                request.get_header_value("Authorization"),
                request.get_header_value("Content-Type"),
            });
            response.status = responseStatus;
            response.set_content(responseBody, "application/json");
        }

        httplib::Server server;
        std::thread listener;
        int port = 0;

        mutable std::mutex mutex;
        std::vector<Request> recorded;
        int responseStatus = 200;
        std::string responseBody;
};
