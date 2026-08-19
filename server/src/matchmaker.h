#pragma once

#include <optional>
#include <utility>
#include <vector>

#include <chess/types.h>
#include <chess/net/messages.h>

#include "client.h"

class Matchmaker {
public:
    std::optional<std::pair<Client*, Client*>> enqueue(Client& client);
    void remove(Client& client);
    bool inQueue(const Client& client) const;

private:
    std::vector<Client*> queue_;
};
