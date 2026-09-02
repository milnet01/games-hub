#include "chessboard.h"

#include <algorithm>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>

namespace chess {

namespace {

// Direction tables. Every piece except the pawn moves along one of these,
// either one step (knight, king) or until something blocks it.
constexpr int kKnightDirs[8][2] = { { -2, -1 }, { -2, 1 }, { -1, -2 }, { -1, 2 },
                                    { 1, -2 },  { 1, 2 },  { 2, -1 },  { 2, 1 } };
constexpr int kBishopDirs[4][2] = { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } };
constexpr int kRookDirs[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
constexpr int kRoyalDirs[8][2] = { { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 },
                                   { 0, 1 },   { 1, -1 }, { 1, 0 },  { 1, 1 } };

// White's pawns move up the screen, towards row 0.
constexpr int forward(Colour c) { return c == Colour::White ? -1 : 1; }
constexpr int homeRank(Colour c) { return c == Colour::White ? 7 : 0; }
constexpr int pawnRank(Colour c) { return c == Colour::White ? 6 : 1; }
constexpr int promotionRank(Colour c) { return c == Colour::White ? 0 : 7; }

char letterFor(Piece p)
{
    char c = '?';
    switch (p.type) {
    case PieceType::Pawn:   c = 'p'; break;
    case PieceType::Knight: c = 'n'; break;
    case PieceType::Bishop: c = 'b'; break;
    case PieceType::Rook:   c = 'r'; break;
    case PieceType::Queen:  c = 'q'; break;
    case PieceType::King:   c = 'k'; break;
    case PieceType::None:   return ' ';
    }
    return p.colour == Colour::White ? char(std::toupper(static_cast<unsigned char>(c))) : c;
}

PieceType typeForLetter(char c)
{
    switch (std::tolower(static_cast<unsigned char>(c))) {
    case 'p': return PieceType::Pawn;
    case 'n': return PieceType::Knight;
    case 'b': return PieceType::Bishop;
    case 'r': return PieceType::Rook;
    case 'q': return PieceType::Queen;
    case 'k': return PieceType::King;
    default:  return PieceType::None;
    }
}

std::vector<std::string> splitFields(const std::string& text)
{
    std::vector<std::string> out;
    std::string current;
    for (char c : text) {
        if (c == ' ' || c == '\t') {
            if (!current.empty())
                out.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty())
        out.push_back(current);
    return out;
}

} // namespace

std::string squareName(Square s)
{
    if (!s.valid())
        return "-";
    std::string out;
    out += char('a' + s.col);
    out += char('0' + (kRanks - s.row));
    return out;
}

// ---------------------------------------------------------------------------
// Position
// ---------------------------------------------------------------------------

void Board::reset()
{
    setFromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

namespace {

// One numeric FEN field, bounded. std::atoi cannot report failure and cannot be
// bounded, and both matter here: see the halfmove clock below.
int numericField(const std::string& text, int low, int high, int fallback)
{
    int value = 0;
    const char* const first = text.data();
    const char* const last = first + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc {} || ptr != last)
        return fallback;
    return std::clamp(value, low, high);
}

} // namespace

bool Board::setFromFen(const std::string& fen)
{
    const std::vector<std::string> fields = splitFields(fen);
    if (fields.size() < 2)
        return false;

    std::array<Piece, kSquares> cells {};
    int row = 0;
    int col = 0;
    for (char c : fields[0]) {
        if (c == '/') {
            if (col != kFiles)
                return false;
            ++row;
            col = 0;
            continue;
        }
        if (c >= '1' && c <= '8') {
            col += c - '0';
            if (col > kFiles)
                return false;
            continue;
        }
        const PieceType type = typeForLetter(c);
        if (type == PieceType::None || row >= kRanks || col >= kFiles)
            return false;
        const Colour side = std::isupper(static_cast<unsigned char>(c)) ? Colour::White
                                                                        : Colour::Black;
        cells[row * kFiles + col] = { type, side };
        ++col;
    }
    if (row != kRanks - 1 || col != kFiles)
        return false;

    // Exactly one king a side. The geometry above says the letters fit the
    // board; it says nothing about whether they make a POSITION, and a board
    // with no black king can never be checkmate -- so a game derived from a bad
    // FEN would run for ever with the engine unable to see the end of it.
    int kings[2] = { 0, 0 };
    for (const Piece& p : cells)
        if (p.type == PieceType::King)
            ++kings[int(p.colour)];
    if (kings[0] != 1 || kings[1] != 1)
        return false;

    // Everything below lands on a CANDIDATE rather than on this board, so a FEN
    // refused at the last test leaves the position already loaded alone -- the
    // rule every restoreState() here follows.
    // COPIED, never default-constructed: Board() calls reset(), and reset()
    // calls this function. A fresh one here recurses until the stack runs out.
    // Every field below is overwritten, so what it starts from does not matter.
    Board candidate = *this;
    candidate.m_cells = cells;
    candidate.m_toMove = fields[1] == "b" ? Colour::Black : Colour::White;

    const std::string rights = fields.size() > 2 ? fields[2] : "-";
    candidate.m_castle[0][0] = rights.find('K') != std::string::npos;
    candidate.m_castle[0][1] = rights.find('Q') != std::string::npos;
    candidate.m_castle[1][0] = rights.find('k') != std::string::npos;
    candidate.m_castle[1][1] = rights.find('q') != std::string::npos;

    candidate.m_ep = { -1, -1 };
    if (fields.size() > 3 && fields[3].size() == 2 && fields[3][0] != '-') {
        const int epCol = fields[3][0] - 'a';
        const int epRow = kRanks - (fields[3][1] - '0');
        if (Square { epRow, epCol }.valid())
            candidate.m_ep = { epRow, epCol };
    }

    // Bounded, and not through atoi. That answers 0 for anything it cannot read
    // and has no bound at all, so a NEGATIVE halfmove clock walked straight in
    // and could never climb to the hundred the fifty-move draw needs -- turning
    // that rule off with nothing to say so.
    candidate.m_halfmove = fields.size() > 4 ? numericField(fields[4], 0, 9999, 0) : 0;
    candidate.m_fullmove = fields.size() > 5 ? numericField(fields[5], 1, 9999, 1) : 1;

    // The side that is NOT to move must not be in check: it would mean the
    // player who just moved had left their own king attacked, which is not a
    // position the rules can reach.
    if (candidate.inCheck(other(candidate.m_toMove)))
        return false;

    *this = candidate;
    return true;
}

std::string Board::positionKey() const
{
    std::string out;
    for (int row = 0; row < kRanks; ++row) {
        int gap = 0;
        for (int col = 0; col < kFiles; ++col) {
            const Piece p = at(row, col);
            if (p.empty()) {
                ++gap;
                continue;
            }
            if (gap) {
                out += char('0' + gap);
                gap = 0;
            }
            out += letterFor(p);
        }
        if (gap)
            out += char('0' + gap);
        if (row != kRanks - 1)
            out += '/';
    }

    out += m_toMove == Colour::White ? " w " : " b ";
    std::string rights;
    if (m_castle[0][0]) rights += 'K';
    if (m_castle[0][1]) rights += 'Q';
    if (m_castle[1][0]) rights += 'k';
    if (m_castle[1][1]) rights += 'q';
    out += rights.empty() ? "-" : rights;
    out += ' ';
    out += squareName(m_ep);
    return out;
}

std::string Board::fen() const
{
    return positionKey() + " " + std::to_string(m_halfmove) + " " + std::to_string(m_fullmove);
}

int Board::pieceCount(Colour c) const
{
    int n = 0;
    for (const Piece& p : m_cells)
        if (p.is(c))
            ++n;
    return n;
}

int Board::material(Colour c) const
{
    static constexpr int kValue[] = { 0, 1, 3, 3, 5, 9, 0 };
    int total = 0;
    for (const Piece& p : m_cells)
        if (p.is(c))
            total += kValue[int(p.type)];
    return total;
}

Square Board::kingSquare(Colour c) const
{
    for (int row = 0; row < kRanks; ++row)
        for (int col = 0; col < kFiles; ++col)
            if (const Piece p = at(row, col); p.is(c) && p.type == PieceType::King)
                return { row, col };
    return { -1, -1 };
}

// ---------------------------------------------------------------------------
// Attacks
// ---------------------------------------------------------------------------

bool Board::squareAttacked(Square s, Colour by) const
{
    if (!s.valid())
        return false;

    // Pawns. A white pawn stands one row *below* what it attacks.
    const int pawnRow = s.row - forward(by);
    for (int dc : { -1, 1 }) {
        const Square from { pawnRow, s.col + dc };
        if (from.valid()) {
            const Piece p = at(from);
            if (p.is(by) && p.type == PieceType::Pawn)
                return true;
        }
    }

    for (const auto& d : kKnightDirs) {
        const Square from { s.row + d[0], s.col + d[1] };
        if (!from.valid())
            continue;
        const Piece p = at(from);
        if (p.is(by) && p.type == PieceType::Knight)
            return true;
    }

    for (const auto& d : kRoyalDirs) {
        const Square from { s.row + d[0], s.col + d[1] };
        if (!from.valid())
            continue;
        const Piece p = at(from);
        if (p.is(by) && p.type == PieceType::King)
            return true;
    }

    // Sliding pieces: walk out from the square until something blocks.
    const auto scan = [&](const int (*dirs)[2], int count, PieceType straight) {
        for (int i = 0; i < count; ++i) {
            int row = s.row + dirs[i][0];
            int col = s.col + dirs[i][1];
            while (Square { row, col }.valid()) {
                const Piece p = at(row, col);
                if (!p.empty()) {
                    if (p.is(by) && (p.type == straight || p.type == PieceType::Queen))
                        return true;
                    break;
                }
                row += dirs[i][0];
                col += dirs[i][1];
            }
        }
        return false;
    };

    return scan(kBishopDirs, 4, PieceType::Bishop) || scan(kRookDirs, 4, PieceType::Rook);
}

bool Board::inCheck(Colour c) const
{
    const Square king = kingSquare(c);
    return king.valid() && squareAttacked(king, other(c));
}

// ---------------------------------------------------------------------------
// Move generation
// ---------------------------------------------------------------------------

void Board::addStepMoves(Square from, Colour c, const int (*dirs)[2], int count, bool sliding,
                         std::vector<Move>& out) const
{
    for (int i = 0; i < count; ++i) {
        int row = from.row + dirs[i][0];
        int col = from.col + dirs[i][1];
        while (Square { row, col }.valid()) {
            const Piece target = at(row, col);
            if (target.is(c))
                break;
            out.push_back({ from, { row, col } });
            if (!target.empty() || !sliding)
                break;
            row += dirs[i][0];
            col += dirs[i][1];
        }
    }
}

void Board::addPawnMoves(Square from, Colour c, std::vector<Move>& out) const
{
    const int dr = forward(c);
    const auto push = [&](Square to, bool enPassant) {
        if (to.row == promotionRank(c)) {
            for (PieceType promo : { PieceType::Queen, PieceType::Rook, PieceType::Bishop,
                                     PieceType::Knight })
                out.push_back({ from, to, promo, false, false });
        } else {
            out.push_back({ from, to, PieceType::None, enPassant, false });
        }
    };

    const Square ahead { from.row + dr, from.col };
    if (ahead.valid() && at(ahead).empty()) {
        push(ahead, false);
        const Square twoAhead { from.row + 2 * dr, from.col };
        if (from.row == pawnRank(c) && twoAhead.valid() && at(twoAhead).empty())
            out.push_back({ from, twoAhead });
    }

    for (int dc : { -1, 1 }) {
        const Square to { from.row + dr, from.col + dc };
        if (!to.valid())
            continue;
        if (at(to).is(other(c)))
            push(to, false);
        else if (to == m_ep && at(to).empty())
            push(to, true);
    }
}

void Board::addCastlingMoves(Colour c, std::vector<Move>& out) const
{
    const int row = homeRank(c);
    const Square king { row, 4 };
    if (!(at(king).is(c) && at(king).type == PieceType::King))
        return;
    // Castling out of check is illegal, and it is cheaper to ask once here
    // than per side.
    if (squareAttacked(king, other(c)))
        return;

    // { rook file, files that must be empty, the file the king crosses,
    //   the file the king lands on }
    struct Side { int rookCol; int emptyFrom; int emptyTo; int crossCol; int kingCol; };
    const Side sides[2] = { { 7, 5, 6, 5, 6 }, { 0, 1, 3, 3, 2 } };

    for (int i = 0; i < 2; ++i) {
        if (!m_castle[int(c)][i])
            continue;
        const Side& s = sides[i];
        const Piece rook = at(row, s.rookCol);
        if (!(rook.is(c) && rook.type == PieceType::Rook))
            continue;

        bool clear = true;
        for (int col = s.emptyFrom; col <= s.emptyTo && clear; ++col)
            clear = at(row, col).empty();
        if (!clear)
            continue;
        // The square the king crosses must be safe; the one it lands on is
        // covered by the legality filter every move goes through.
        if (squareAttacked({ row, s.crossCol }, other(c)))
            continue;

        out.push_back({ king, { row, s.kingCol }, PieceType::None, false, true });
    }
}

std::vector<Move> Board::pseudoMoves(Colour c) const
{
    std::vector<Move> out;
    out.reserve(48);

    for (int row = 0; row < kRanks; ++row) {
        for (int col = 0; col < kFiles; ++col) {
            const Piece p = at(row, col);
            if (!p.is(c))
                continue;
            const Square from { row, col };
            switch (p.type) {
            case PieceType::Pawn:   addPawnMoves(from, c, out); break;
            case PieceType::Knight: addStepMoves(from, c, kKnightDirs, 8, false, out); break;
            case PieceType::Bishop: addStepMoves(from, c, kBishopDirs, 4, true, out); break;
            case PieceType::Rook:   addStepMoves(from, c, kRookDirs, 4, true, out); break;
            case PieceType::Queen:  addStepMoves(from, c, kRoyalDirs, 8, true, out); break;
            case PieceType::King:   addStepMoves(from, c, kRoyalDirs, 8, false, out); break;
            case PieceType::None:   break;
            }
        }
    }

    addCastlingMoves(c, out);
    return out;
}

std::vector<Move> Board::legalMoves(Colour c) const
{
    std::vector<Move> out;
    out.reserve(48);
    for (const Move& m : pseudoMoves(c)) {
        Board next = *this;
        next.apply(m);
        if (!next.inCheck(c))
            out.push_back(m);
    }
    return out;
}

std::vector<Move> Board::legalCaptures(Colour c) const
{
    std::vector<Move> out;
    out.reserve(12);
    for (const Move& m : pseudoMoves(c)) {
        // The cheap test first. This is the same question isNoisy() asks in the
        // search, and asking it before the board copy is the point.
        if (at(m.to).empty() && !m.enPassant && m.promotion == PieceType::None)
            continue;
        Board next = *this;
        next.apply(m);
        if (!next.inCheck(c))
            out.push_back(m);
    }
    return out;
}

void Board::apply(const Move& m)
{
    const Piece mover = at(m.from);
    const bool capture = !at(m.to).empty() || m.enPassant;

    if (m.enPassant)
        set(m.from.row, m.to.col, {});

    if (m.castle) {
        const int rookFrom = m.to.col > m.from.col ? 7 : 0;
        const int rookTo = m.to.col > m.from.col ? 5 : 3;
        set(m.from.row, rookTo, at(m.from.row, rookFrom));
        set(m.from.row, rookFrom, {});
    }

    set(m.from, {});
    set(m.to, m.promotion == PieceType::None ? mover : Piece { m.promotion, mover.colour });

    // Rights are lost by moving the king or a rook, and by having a rook taken
    // on the square it started from.
    if (mover.type == PieceType::King)
        m_castle[int(mover.colour)][0] = m_castle[int(mover.colour)][1] = false;
    const auto clearRookRight = [&](Square s) {
        for (Colour c : { Colour::White, Colour::Black }) {
            if (s.row != homeRank(c))
                continue;
            if (s.col == 7)
                m_castle[int(c)][0] = false;
            else if (s.col == 0)
                m_castle[int(c)][1] = false;
        }
    };
    if (mover.type == PieceType::Rook)
        clearRookRight(m.from);
    clearRookRight(m.to);

    // A double step only offers en passant when a pawn is actually there to
    // take it, which keeps the position key honest for repetition claims.
    m_ep = { -1, -1 };
    if (mover.type == PieceType::Pawn && std::abs(m.to.row - m.from.row) == 2) {
        const Square skipped { (m.to.row + m.from.row) / 2, m.from.col };
        for (int dc : { -1, 1 }) {
            const Square beside { m.to.row, m.to.col + dc };
            if (!beside.valid())
                continue;
            const Piece p = at(beside);
            if (p.is(other(mover.colour)) && p.type == PieceType::Pawn) {
                m_ep = skipped;
                break;
            }
        }
    }

    m_halfmove = (mover.type == PieceType::Pawn || capture) ? 0 : m_halfmove + 1;
    if (m_toMove == Colour::Black)
        ++m_fullmove;
    m_toMove = other(m_toMove);
}

bool Board::isCheckmate() const
{
    return inCheck(m_toMove) && legalMoves(m_toMove).empty();
}

bool Board::isStalemate() const
{
    return !inCheck(m_toMove) && legalMoves(m_toMove).empty();
}

bool Board::insufficientMaterial() const
{
    int knights = 0;
    int bishops = 0;
    int bishopSquareColours = 0;   // bit 0 = a light bishop, bit 1 = a dark one

    for (int row = 0; row < kRanks; ++row) {
        for (int col = 0; col < kFiles; ++col) {
            switch (at(row, col).type) {
            case PieceType::Pawn:
            case PieceType::Rook:
            case PieceType::Queen:
                return false;
            case PieceType::Knight:
                ++knights;
                break;
            case PieceType::Bishop:
                ++bishops;
                bishopSquareColours |= 1 << ((row + col) % 2);
                break;
            default:
                break;
            }
        }
    }

    if (knights + bishops == 0)     // bare kings
        return true;
    if (knights == 1 && bishops == 0)
        return true;
    // Bishops confined to one colour of square can never deliver mate, however
    // many there are or whoever owns them.
    return knights == 0 && bishopSquareColours != 3;
}

std::string Board::notation(const Move& m) const
{
    if (m.castle)
        return m.to.col > m.from.col ? "O-O" : "O-O-O";

    const Piece mover = at(m.from);
    std::string out;
    if (mover.type != PieceType::Pawn)
        out += char(std::toupper(
            static_cast<unsigned char>(letterFor({ mover.type, Colour::White }))));
    out += squareName(m.from);
    out += (!at(m.to).empty() || m.enPassant) ? 'x' : '-';
    out += squareName(m.to);
    if (m.promotion != PieceType::None) {
        out += '=';
        out += char(std::toupper(
            static_cast<unsigned char>(letterFor({ m.promotion, Colour::White }))));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Game
// ---------------------------------------------------------------------------

void ChessGame::reset()
{
    m_board.reset();
    m_past.clear();
    m_played.clear();
    m_keys.assign(1, m_board.positionKey());
}

bool ChessGame::setFromFen(const std::string& fen)
{
    Board board;
    if (!board.setFromFen(fen))
        return false;
    m_board = board;
    m_past.clear();
    m_played.clear();
    m_keys.assign(1, m_board.positionKey());
    return true;
}

void ChessGame::play(const Move& m)
{
    m_past.push_back(m_board);
    m_played.push_back(m);
    m_board.apply(m);
    m_keys.push_back(m_board.positionKey());
}

bool ChessGame::undo()
{
    if (m_past.empty())
        return false;
    m_board = m_past.back();
    m_past.pop_back();
    m_played.pop_back();
    m_keys.pop_back();
    return true;
}

int ChessGame::repetitionCount() const
{
    const std::string& current = m_keys.back();
    return int(std::count(m_keys.begin(), m_keys.end(), current));
}

Result ChessGame::result() const
{
    if (m_board.legalMoves().empty())
        return m_board.inCheck() ? (m_board.toMove() == Colour::White ? Result::BlackWins
                                                                     : Result::WhiteWins)
                                 : Result::Draw;
    if (m_board.insufficientMaterial() || m_board.halfmoveClock() >= 100
        || repetitionCount() >= 3)
        return Result::Draw;
    return Result::Playing;
}

DrawReason ChessGame::drawReason() const
{
    if (result() != Result::Draw)
        return DrawReason::None;
    if (m_board.legalMoves().empty())
        return DrawReason::Stalemate;
    if (m_board.insufficientMaterial())
        return DrawReason::InsufficientMaterial;
    if (repetitionCount() >= 3)
        return DrawReason::Repetition;
    return DrawReason::FiftyMove;
}

} // namespace chess
