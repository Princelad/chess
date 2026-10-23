#include "matchmaker.h"

#include <algorithm>

#include "client.h"
#include "log.h"

std::optional<std::pair<Client*, Client*>> Matchmaker::enqueue(Client& client)
{
    for (auto* c : queue_) {
        if (c == &client)
            return std::nullopt;
    }

    queue_.push_back(&client);

    // Pair the first queued player with the same time control (11.1) and
    // the same game variant (12.1: standard and Chess960 never cross); a
    // "5+3"≠"10+0" or standard≠chess960 disagreement stays queued. The
    // earlier entry plays White, as before.
    Client* other = nullptr;
    for (auto* c : queue_)
        if (c != &client && c->timeControl == client.timeControl
            && c->variant == client.variant) {
            other = c;
            break;
        }
    if (other == nullptr)
        return std::nullopt;

    // Both sides leave the queue; the caller was pushed last moments ago,
    // so pop it instead of a scan-with-erase.
    queue_.pop_back();
    queue_.erase(std::remove(queue_.begin(), queue_.end(), other),
                 queue_.end());

    LOG_INFO("Matched: " + other->name + " (White) vs " + client.name
             + " (Black)" + (other->timeControl.empty()
                ? " (unlimited)"
                : " (" + other->timeControl + ")")
             + (other->variant.empty() ? std::string()
                : " [" + other->variant + "]"));

    return std::make_pair(other, &client);
}

void Matchmaker::remove(Client& client)
{
    queue_.erase(
        std::remove(queue_.begin(), queue_.end(), &client),
        queue_.end());
}
