#pragma once

#include "app.h"
#include "archive_pgn.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/panel.h"

#include <chess/board.h>
#include <chess/move.h>
#include <chess/san.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace chess::client {

// Match-archive browser (10.3): requests ArchiveListMsg on open, shows the
// rows ("Ada vs Bob · 1-0 · date"), and offers per-game detail with replay
// ("Watch") and PGN export ("Copy PGN"). Assumes an established connection;
// the server answers requests in any connection state.
class HistoryScreen : public Screen {
public:
    HistoryScreen(App& app, bool mineOnlyDefault);
    void handleEvent(const sf::Event& event) override;
    void update(float dtSec) override;
    void draw(sf::RenderWindow& window) override;

private:
    enum class Phase { Busy, List, Detail };

    void request();
    void openDetail(std::int64_t id);
    void backToList();
    void copyPgn();
    void watchGame();
    void layoutWidgets();

    // Rebuilds the playable move chain from the record (AnalysisScreen input).
    struct Replay {
        Board initialBoard;
        std::vector<chess::Move> moves;
        std::vector<std::string> sans;
        std::string resultText;
    };
    static Replay buildReplay(const chess::net::ArchiveGameMsg& record);

    App& app_;
    Phase phase_ = Phase::Busy;
    bool mineOnly_ = false;
    bool mineToggleable_ = false;
    std::vector<chess::net::ArchiveEntryMsg> entries_;
    std::optional<chess::net::ArchiveGameMsg> record_;
    std::size_t topRow_ = 0;      // first visible entry (window scroll)

    static constexpr int PageRows = 15;

    Panel bg_;
    Panel detailPanel_;
    Label title_;
    Label status_;
    Label detailTitle_;
    Label detailMeta_;
    Label detailSans_;
    Checkbox mineCheck_;
    Button backBtn_;
    Button watchBtn_;
    Button copyBtn_;
    std::vector<Button> rowButtons_;
};

} // namespace chess::client
