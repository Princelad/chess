#pragma once

#include <optional>
#include <utility>
#include <vector>

struct Client;

class Matchmaker {
public:
    std::optional<std::pair<Client*, Client*>> enqueue(Client& client);
    void remove(Client& client);

private:
    std::vector<Client*> queue_;
};
