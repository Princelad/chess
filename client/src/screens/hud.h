#pragma once

#include "widgets/label.h"
#include "widgets/move_navigator.h"
#include "widgets/panel.h"
#include <chess/move.h>
#include <chess/types.h>

#include <cstddef>
#include <string>
#include <vector>

namespace chess::client {

class App;

class Hud {
public:
    Hud(App& app, float panelX, float panelWidth);

    void setGame(const Board& initialBoard, std::vector<chess::Move> moves,
                 std::vector<std::string> sans);
    void appendMove(const chess::Move& move, const std::string& san);
    MoveNavigator& navigator() { return navigator_; }

    void setInfo(const std::string& opponentName, Color myColor,
                 bool myTurn, bool gameOver);
    void setStatus(const std::string& msg, float duration);
    void setGameOver(bool gameOver) { gameOver_ = gameOver; }
    void handleScroll(float delta) { navigator_.handleScroll(delta); }
    void update(float dtSec);
    void draw(sf::RenderWindow& window, const sf::Font& font);

    float contentBottom() const;
    float moveListBottom() const;

private:
    void drawPlayerCard(sf::RenderWindow& window, const sf::Font& font,
                        const std::string& name, Color cardColor,
                        float nameY, float capturedY, bool isLocal);

    App& app_;
    float panelX_ = 0.f;
    float panelW_ = 0.f;

    std::string opponentName_;
    Color myColor_ = Color::White;
    bool myTurn_ = false;
    bool gameOver_ = false;

    MoveNavigator navigator_;

    std::string statusMsg_;
    float statusTimer_ = 0.f;

    Label opponentNameLabel_;
    Label localNameLabel_;
    Label statusLabel_;
    Label headerLabel_;
};

} // namespace chess::client