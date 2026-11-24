#pragma once

// Audit 10.1: the draw / takeback offer prompts share one accept/decline
// row in GameScreen, but their LIFETIME belongs to the server. The server
// dissolves a pending offer whenever the position moves on (an accepted
// move) or the conversation loses a seat (a disconnect), and it never
// sends a notice for that. The client used to keep the prompt forever:
// the action row stayed replaced by dead Accept/Decline buttons and the
// player could not resign, offer a draw, or ask for a takeback again.
//
// This is the pure reducer that mirrors the server's offer lifecycle.
// Rendering precedence is draw-wins while both are pending (the same row
// answers them one at a time, oldest first by convention).

namespace chess::client::offer {

struct State {
    bool draw = false;
    bool takeback = false;

    constexpr bool empty() const { return !draw && !takeback; }

    explicit constexpr operator bool() const { return !empty(); }

    constexpr bool operator==(const State& other) const
    {
        return draw == other.draw && takeback == other.takeback;
    }
    constexpr bool operator!=(const State& other) const
    {
        return !(*this == other);
    }
};

enum class Event {
    DrawOffered,
    DrawDeclined,
    TakebackRequested,
    TakebackDeclined,
    // The server dissolved whatever was pending: an accepted move moved
    // the position on.
    PositionChanged,
    // The opponent dropped (the server clears offers on disconnect).
    SeatReset,
    GameEnded,
};

struct Result {
    State state;
    // True when this event cleared a prompt that was actually showing;
    // callers echo the withdrawal into the chat log so the row's
    // disappearance is explained.
    bool dissolvedDraw = false;
    bool dissolvedTakeback = false;
};

constexpr Result apply(State state, Event event)
{
    Result result{state, false, false};
    switch (event) {
        case Event::DrawOffered:
            result.state.draw = true;
            break;
        case Event::DrawDeclined:
            result.dissolvedDraw = result.state.draw;
            result.state.draw = false;
            break;
        case Event::TakebackRequested:
            result.state.takeback = true;
            break;
        case Event::TakebackDeclined:
            result.dissolvedTakeback = result.state.takeback;
            result.state.takeback = false;
            break;
        case Event::PositionChanged:
        case Event::SeatReset:
        case Event::GameEnded:
            result.dissolvedDraw = result.state.draw;
            result.dissolvedTakeback = result.state.takeback;
            result.state = State{};
            break;
    }
    return result;
}

// The row renders one prompt at a time; a draw offer takes precedence for
// as long as it is pending.
constexpr bool visible(const State& state) { return !state.empty(); }
constexpr bool showsDraw(const State& state) { return state.draw; }
constexpr bool showsTakeback(const State& state)
{
    return !state.draw && state.takeback;
}

} // namespace chess::client::offer
