#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <SFML/Network/TcpSocket.hpp>

struct Client {
    std::unique_ptr<sf::TcpSocket> socket;
    std::string name;
    std::chrono::steady_clock::time_point lastActivity;
};
