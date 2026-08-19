#include "matchmaker.h"

#include <algorithm>
#include <iostream>

#include "client.h"

std::optional<std::pair<Client*, Client*>> Matchmaker::enqueue(Client& client)
{
    for (auto* c : queue_) {
        if (c == &client)
            return std::nullopt;
    }

    queue_.push_back(&client);

    if (queue_.size() < 2)
        return std::nullopt;

    Client* white = queue_[0];
    Client* black = queue_[1];
    queue_.clear();

    std::cout << "[INFO] Matched: " << white->name << " (White) vs "
              << black->name << " (Black)\n";

    return std::make_pair(white, black);
}

void Matchmaker::remove(Client& client)
{
    queue_.erase(
        std::remove(queue_.begin(), queue_.end(), &client),
        queue_.end());
}
