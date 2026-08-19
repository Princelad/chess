#pragma once

#include <chess/net/protocol.h>

#include "client.h"

inline bool sendTo(Client& client, const chess::net::ServerMessage& msg)
{
    sf::Packet packet;
    chess::net::serialize(packet, msg);
    return client.socket->send(packet) == sf::Socket::Status::Done;
}
