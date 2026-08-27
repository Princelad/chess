#pragma once

#include <chess/net/protocol.h>

#include "client.h"

inline bool sendTo(Client& client, const chess::net::ServerMessage& msg)
{
    if (!client.socket) return client.isBot;
    sf::Packet packet;
    chess::net::serialize(packet, msg);
    auto status = client.socket->send(packet);
    if (status != sf::Socket::Status::Done) {
        client.socket->disconnect();
        return false;
    }
    return true;
}
