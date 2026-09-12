#include "move_navigator.h"
#include "app.h"

#include <algorithm>
#include <cmath>

namespace chess::client {

namespace {
constexpr float GridPadLeft = 6.f;
constexpr float NumColW = 42.f;
constexpr float ColGap = 8.f;
constexpr float CellPad = 4.f;
constexpr float FontSize = 13.f;
constexpr float RowH = 16.f;

std::string truncateUtf8(const std::string& s, std::size_t maxBytes)
{
    if (s.size() <= maxBytes) return s;
    std::size_t n = maxBytes;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    return s.substr(0, n > 0 ? n - 1 : 0) + "...";
}

int pieceValue(PieceType type)
{
    switch (type) {
        case PieceType::Pawn:   return 1;
        case PieceType::Knight: return 3;
        case PieceType::Bishop: return 3;
        case PieceType::Rook:   return 5;
        case PieceType::Queen:  return 9;
        default:                return 0;
    }
}

void sortPieces(CapturedMaterial& mat)
{
    std::sort(mat.pieces.begin(), mat.pieces.end(),
              [](const Piece& a, const Piece& b) {
                  int va = pieceValue(a.type);
                  int vb = pieceValue(b.type);
                  if (va != vb) return va < vb;
                  return static_cast<int>(a.type) < static_cast<int>(b.type);
              });
}

void recordCapture(CapturedMaterial& mat, Piece captured)
{
    if (captured.isNone()) return;
    mat.counts[static_cast<int>(captured.type)]++;
    mat.value += pieceValue(captured.type);
    mat.pieces.push_back(captured);
}

int visibleLines(float listHeight)
{
    return std::max(1, static_cast<int>((listHeight - 2.f) / RowH));
}
}

void MoveNavigator::setGame(Board initialBoard, std::vector<chess::Move> moves,
                            std::vector<std::string> sans)
{
    initialBoard_ = std::move(initialBoard);
    moves_ = std::move(moves);
    sans_ = std::move(sans);
    currentPly_ = 0;
    moveScroll_ = 0;
    rebuildFromMoves();
    replayTo(0);
    updateNavButtons();
    keepCurrentPlyVisible();
}

void MoveNavigator::appendMove(const chess::Move& m, const std::string& san)
{
    Color mover = finalBoard_.pieceAt(m.from).color;
    if (m.isEnPassant()) {
        recordCapture(captured_[static_cast<int>(mover)],
                      Piece::of(opposite(mover), PieceType::Pawn));
    } else if (m.isCapture()) {
        recordCapture(captured_[static_cast<int>(mover)], finalBoard_.pieceAt(m.to));
    }
    sortPieces(captured_[static_cast<int>(mover)]);

    const bool wasAtEnd = atEnd();
    finalBoard_.makeMove(m);
    moves_.push_back(m);
    sans_.push_back(san);

    if (wasAtEnd) {
        currentPly_ = totalPlies();
        replayTo(currentPly_);
        keepCurrentPlyVisible();
    }
    updateNavButtons();
}

void MoveNavigator::goToPly(int ply)
{
    int clamped = std::clamp(ply, 0, totalPlies());
    if (clamped == currentPly_) return;
    currentPly_ = clamped;
    replayTo(currentPly_);
    updateNavButtons();
    keepCurrentPlyVisible();
}

void MoveNavigator::rebuildFromMoves()
{
    captured_ = {};
    finalBoard_ = initialBoard_;
    for (const auto& m : moves_) {
        Color mover = finalBoard_.pieceAt(m.from).color;
        if (m.isEnPassant()) {
            recordCapture(captured_[static_cast<int>(mover)],
                          Piece::of(opposite(mover), PieceType::Pawn));
        } else if (m.isCapture()) {
            recordCapture(captured_[static_cast<int>(mover)], finalBoard_.pieceAt(m.to));
        }
        finalBoard_.makeMove(m);
    }
    for (auto& mat : captured_) sortPieces(mat);
}

void MoveNavigator::replayTo(int ply)
{
    board_ = initialBoard_;
    for (int i = 0; i < ply; ++i) board_.makeMove(moves_[i]);
}

std::optional<std::pair<int, int>> MoveNavigator::lastMoveFrom() const
{
    if (currentPly_ == 0) return std::nullopt;
    return std::make_pair(
        static_cast<int>(chess::fileOf(moves_[currentPly_ - 1].from)),
        static_cast<int>(chess::rankOf(moves_[currentPly_ - 1].from)));
}

std::optional<std::pair<int, int>> MoveNavigator::lastMoveTo() const
{
    if (currentPly_ == 0) return std::nullopt;
    return std::make_pair(
        static_cast<int>(chess::fileOf(moves_[currentPly_ - 1].to)),
        static_cast<int>(chess::rankOf(moves_[currentPly_ - 1].to)));
}

int MoveNavigator::materialAdvantage(Color color) const
{
    return captured_[static_cast<int>(color)].value
        - captured_[static_cast<int>(opposite(color))].value;
}

void MoveNavigator::setLayout(sf::FloatRect gridRect, sf::FloatRect navRect)
{
    gridRect_ = gridRect;
    navRect_ = navRect;

    const float gap = 6.f;
    float w = (navRect_.size.x - 3.f * gap) / 4.f;
    for (int i = 0; i < 4; ++i) {
        navButtons_[i].setRect(sf::FloatRect(
            sf::Vector2f(navRect_.position.x + static_cast<float>(i) * (w + gap),
                         navRect_.position.y),
            sf::Vector2f(w, navRect_.size.y)));
    }
}

void MoveNavigator::handleScroll(float delta)
{
    moveScroll_ -= static_cast<int>(delta);
    clampScroll();
}

void MoveNavigator::clampScroll()
{
    int vis = visibleLines(gridRect_.size.y);
    int totalPairs = (totalPlies() + 1) / 2;
    int maxScroll = std::max(0, totalPairs - vis);
    moveScroll_ = std::clamp(moveScroll_, 0, maxScroll);
}

void MoveNavigator::updateNavButtons()
{
    navButtons_[0].setEnabled(!atStart());
    navButtons_[1].setEnabled(!atStart());
    navButtons_[2].setEnabled(!atEnd());
    navButtons_[3].setEnabled(!atEnd());
}

void MoveNavigator::keepCurrentPlyVisible()
{
    int vis = visibleLines(gridRect_.size.y);
    int pairIdx = currentPly_ / 2;
    if (pairIdx > moveScroll_ + vis - 1)
        moveScroll_ = pairIdx - vis + 1;
    if (pairIdx < moveScroll_)
        moveScroll_ = pairIdx;
    clampScroll();
}

int MoveNavigator::cellPlyAt(sf::Vector2f local) const
{
    if (!gridRect_.contains(local)) return -1;

    float top = gridRect_.position.y + 2.f;
    int row = static_cast<int>((local.y - top) / RowH);
    if (row < 0) return 0;

    int gIdx = moveScroll_ + row;
    int whitePly = gIdx * 2;
    int blackPly = gIdx * 2 + 1;

    float px = gridRect_.position.x;
    float colW = (gridRect_.size.x - GridPadLeft - NumColW - ColGap) / 2.f;
    float whiteX = px + GridPadLeft + NumColW + CellPad;
    float blackX = whiteX + colW + ColGap;
    float yTop = top + static_cast<float>(row) * RowH;

    if (local.x >= whiteX && local.x < whiteX + colW)
        return whitePly;
    if (local.x >= blackX && local.x < blackX + colW)
        return blackPly;
    return 0;
}

void MoveNavigator::updateHover(sf::Vector2f local)
{
    if (gridRect_.contains(local)) {
        int ply = cellPlyAt(local);
        hoveredCellPly_ = ply;
    } else {
        hoveredCellPly_ = -1;
    }
}

bool MoveNavigator::handleEvent(const sf::Event& event, const sf::Vector2f& local)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        switch (kp->code) {
            case sf::Keyboard::Key::Left:
                goBack();
                return true;
            case sf::Keyboard::Key::Right:
                goForward();
                return true;
            case sf::Keyboard::Key::Home:
                goStart();
                return true;
            case sf::Keyboard::Key::End:
                goEnd();
                return true;
            default:
                return false;
        }
    }

    for (auto& btn : navButtons_) {
        if (btn.handleEvent(event, local)) return true;
    }

    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        (void)mm;
        updateHover(local);
        return false;
    }
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button != sf::Mouse::Button::Left) return false;
        if (!gridRect_.contains(local)) return false;
        int ply = cellPlyAt(local);
        if (ply > 0 && ply <= totalPlies()) goToPly(ply);
        return true;
    }
    if (const auto* rb = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (rb->button != sf::Mouse::Button::Left) return false;
        return gridRect_.contains(local);
    }
    return false;
}

void MoveNavigator::draw(sf::RenderWindow& window, const sf::Font& font, const App& app)
{
    drawGrid(window, font);
    drawNavButtons(window, font);
}

void MoveNavigator::drawGrid(sf::RenderWindow& window, const sf::Font& font)
{
    float listH = gridRect_.size.y;
    if (listH <= 0.f) return;

    sf::RectangleShape bg(gridRect_.size);
    bg.setPosition(gridRect_.position);
    bg.setFillColor(sf::Color(25, 25, 25));
    bg.setOutlineColor(sf::Color(80, 80, 80));
    bg.setOutlineThickness(1.f);
    window.draw(bg);

    int vis = visibleLines(listH);
    int totalPairs = (totalPlies() + 1) / 2;
    int endPair = std::min(moveScroll_ + vis, totalPairs);

    float top = gridRect_.position.y + 2.f;
    float px = gridRect_.position.x;
    float colW = (gridRect_.size.x - GridPadLeft - NumColW - ColGap) / 2.f;
    float whiteX = px + GridPadLeft + NumColW + CellPad;
    float blackX = whiteX + colW + ColGap;
    float numX = px + GridPadLeft;

    sf::Text numText(font, "", static_cast<unsigned int>(FontSize));
    numText.setFillColor(sf::Color(130, 130, 130));
    sf::Text moveText(font, "", static_cast<unsigned int>(FontSize));
    moveText.setFillColor(sf::Color(210, 210, 210));

    std::size_t maxChars = std::max<std::size_t>(
        4, static_cast<std::size_t>((colW - 8.f) / 6.f));

    for (int r = moveScroll_; r < endPair; ++r) {
        if (static_cast<float>(r - moveScroll_) * RowH + RowH > listH - 2.f)
            break;

        float yTop = top + static_cast<float>(r - moveScroll_) * RowH;
        int whitePly = r * 2;
        int blackPly = r * 2 + 1;

        auto drawCell = [&](int ply, float cellX, const std::string& san) {
            if (ply > totalPlies()) return;

            if (ply == currentPly_) {
                sf::RectangleShape hl({colW, RowH});
                hl.setPosition({cellX - CellPad, yTop});
                hl.setFillColor(sf::Color(0, 120, 215, 100));
                window.draw(hl);
            } else if (ply == hoveredCellPly_ && ply > 0) {
                sf::RectangleShape hov({colW, RowH});
                hov.setPosition({cellX - CellPad, yTop});
                hov.setFillColor(sf::Color(120, 120, 120, 45));
                window.draw(hov);
            }

            if (san.empty()) return;
            std::string text = truncateUtf8(san, maxChars);
            moveText.setString(text);
            moveText.setPosition({cellX, yTop + 1.f});
            window.draw(moveText);
        };

        numText.setString(std::to_string(r + 1) + ".");
        numText.setPosition({numX, yTop + 1.f});
        window.draw(numText);

        std::string whiteSan = (whitePly < static_cast<int>(sans_.size()))
            ? sans_[whitePly] : std::string();
        std::string blackSan = (blackPly < static_cast<int>(sans_.size()))
            ? sans_[blackPly] : std::string();

        drawCell(whitePly, whiteX, whiteSan);
        drawCell(blackPly, blackX, blackSan);
    }

    if (totalPlies() == 0) {
        sf::Text empty(font, "No moves yet", static_cast<unsigned int>(FontSize));
        empty.setFillColor(sf::Color(100, 100, 100));
        empty.setPosition({px + GridPadLeft, top + 2.f});
        window.draw(empty);
    }
}

void MoveNavigator::drawNavButtons(sf::RenderWindow& window, const sf::Font& font)
{
    for (auto& btn : navButtons_) btn.draw(window, font);
}

} // namespace chess::client