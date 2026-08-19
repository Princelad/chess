#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <SFML/Network/SocketSelector.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include <chess/net/protocol.h>

#include "client.h"
#include "matchmaker.h"

using namespace std::chrono_literals;

static std::atomic<bool> running{true};

static void signalHandler(int) { running.store(false, std::memory_order_relaxed); }

static void printUsage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "  --port <N>    Listen port (default: 5555)\n"
              << "  --host <addr> Bind address (default: 0.0.0.0)\n"
              << "  --help        Show this help\n";
}

static bool sendTo(Client& client, const chess::net::ServerMessage& msg)
{
    sf::Packet packet;
    chess::net::serialize(packet, msg);
    return client.socket->send(packet) == sf::Socket::Status::Done;
}

int main(int argc, char* argv[])
{
    unsigned short port = 5555;
    std::string host = "0.0.0.0";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            int p = std::atoi(argv[++i]);
            if (p < 1 || p > 65535) {
                std::cerr << "[ERROR] Port must be 1-65535\n";
                return 1;
            }
            port = static_cast<unsigned short>(p);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    auto ipOpt = sf::IpAddress::fromString(host);
    if (!ipOpt.has_value()) {
        std::cerr << "[ERROR] Invalid address: " << host << "\n";
        return 1;
    }

    sf::TcpListener listener;
    auto status = listener.listen(port, *ipOpt);
    if (status != sf::Socket::Status::Done) {
        std::cerr << "[ERROR] Failed to bind listener on " << host << ":" << port << "\n";
        return 1;
    }
    std::cout << "[INFO] Listening on " << host << ":" << port << "\n";

    sf::SocketSelector selector;
    selector.add(listener);

    std::vector<std::unique_ptr<Client>> clients;

    std::signal(SIGINT, signalHandler);

    while (running) {
        if (!selector.wait(1s))
            continue;

        if (selector.isReady(listener)) {
            auto newClient = std::make_unique<Client>();
            newClient->socket = std::make_unique<sf::TcpSocket>();
            newClient->lastActivity = std::chrono::steady_clock::now();

            if (listener.accept(*newClient->socket) == sf::Socket::Status::Done) {
                auto addr = newClient->socket->getRemoteAddress();
                std::cout << "[INFO] Client connected from "
                          << (addr.has_value() ? addr->toString() : "unknown") << "\n";
                if (!selector.add(*newClient->socket)) {
                    std::cerr << "[WARN] Failed to add client to selector\n";
                    newClient->socket->disconnect();
                    continue;
                }
                clients.push_back(std::move(newClient));
            } else {
                std::cerr << "[WARN] Failed to accept connection\n";
            }
        }

        for (auto it = clients.begin(); it != clients.end(); ) {
            Client& client = **it;
            if (selector.isReady(*client.socket)) {
                sf::Packet packet;
                auto recvStatus = client.socket->receive(packet);

                if (recvStatus == sf::Socket::Status::Done) {
                    client.lastActivity = std::chrono::steady_clock::now();
                    auto msg = chess::net::deserializeClient(packet);
                    if (msg.has_value()) {
                        std::cout << "[INFO] Received: " << chess::net::debugString(*msg) << "\n";
                    } else {
                        std::cout << "[WARN] Failed to deserialize message\n";
                    }
                } else {
                    auto addr = client.socket->getRemoteAddress();
                    std::cout << "[INFO] Client disconnected: "
                              << (addr.has_value() ? addr->toString() : "unknown") << "\n";
                    selector.remove(*client.socket);
                    it = clients.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    std::cout << "[INFO] Shutting down with " << clients.size() << " client(s) connected\n";
    for (auto& client : clients)
        client->socket->disconnect();
    clients.clear();
    listener.close();
    return 0;
}
