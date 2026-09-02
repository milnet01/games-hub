#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Chess with the whole rule set: castling, en passant, promotion, check,
// checkmate, stalemate, and the three draws that need no agreement — fifty
// moves, threefold repetition and insufficient material. No Qt, so the rules
// are testable headlessly like every other core here.
//
// Everything is inside `chess` because the draughts core already owns the
// obvious names — Side, Piece, Square — at global scope, and the self-test
// includes both.
namespace chess {

enum class Colour : std::int8_t { White = 0, Black = 1 };

constexpr Colour other(Colour c) { return c == Colour::White ? Colour::Black : Colour::White; }

enum class PieceType : std::int8_t { None = 0, Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
    PieceType type = PieceType::None;
    Colour colour = Colour::White;

    bool empty() const { return type == PieceType::None; }
    bool is(Colour c) const { return type != PieceType::None && colour == c; }
};

inline constexpr int kFiles = 8;
inline constexpr int kRanks = 8;
inline constexpr int kSquares = kFiles * kRanks;

// Row 0 is rank 8 and row 7 is rank 1, so a row index reads down the screen
// the way the board is drawn. White therefore starts on rows 6 and 7 and its
// pawns move towards row 0.
struct Square {
    int row = 0;
    int col = 0;

    bool valid() const { return row >= 0 && row < kRanks && col >= 0 && col < kFiles; }

    friend bool operator==(Square a, Square b) { return a.row == b.row && a.col == b.col; }
    friend bool operator!=(Square a, Square b) { return !(a == b); }
};

struct Move {
    Square from;
    Square to;
    // Set only when a pawn reaches the far rank; one move is generated per
    // choice of piece, so the four promotions are four distinct moves.
    PieceType promotion = PieceType::None;
    bool enPassant = false;
    bool castle = false;

    friend bool operator==(const Move& a, const Move& b)
    {
        return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
    }
};

// "e2", "g8" — used by the notation helpers and by the FEN reader.
std::string squareName(Square s);

// One position and the rules that act on it. Copyable and free of heap state,
// because the search copies a board per node.
class Board
{
public:
    Board() { reset(); }

    void reset();

    // Reads placement, side to move, castling rights, en passant target and
    // both clocks. Returns false and leaves the board untouched on anything it
    // cannot parse.
    bool setFromFen(const std::string& fen);
    std::string fen() const;
    // Placement, side, rights and en passant only — the part of a position
    // that has to match for a repetition.
    std::string positionKey() const;

    Piece at(int row, int col) const { return m_cells[row * kFiles + col]; }
    Piece at(Square s) const { return at(s.row, s.col); }

    Colour toMove() const { return m_toMove; }
    int halfmoveClock() const { return m_halfmove; }
    // Unused today, and kept: it is a field of the position like the four
    // accessors around it, all of which ARE used, and it is one of the six
    // fields of the FEN this board reads. Removing it alone would leave the
    // position half-describable.
    int fullmoveNumber() const { return m_fullmove; }
    Square enPassantTarget() const { return m_ep; }
    bool canCastle(Colour c, bool kingSide) const { return m_castle[int(c)][kingSide ? 0 : 1]; }

    // Every legal move: pseudo-legal moves minus the ones that leave, or
    // leave standing, the mover's own king in check.
    std::vector<Move> legalMoves(Colour c) const;
    std::vector<Move> legalMoves() const { return legalMoves(m_toMove); }

    void apply(const Move& m);

    bool squareAttacked(Square s, Colour by) const;
    Square kingSquare(Colour c) const;
    bool inCheck(Colour c) const;
    bool inCheck() const { return inCheck(m_toMove); }

    bool isCheckmate() const;
    bool isStalemate() const;
    // Neither side can force mate: bare kings, king and minor, or two bishops
    // of the same square colour.
    bool insufficientMaterial() const;

    int pieceCount(Colour c) const;
    // Material in pawns, the usual 1/3/3/5/9. Kings score nothing.
    int material(Colour c) const;

    // Long notation — "e2-e4", "Ng1-f3", "e7-e8=Q", "O-O". Not SAN: this only
    // has to be readable in a status line, and SAN's disambiguation rules earn
    // nothing here.
    std::string notation(const Move& m) const;

private:
    void set(int row, int col, Piece p) { m_cells[row * kFiles + col] = p; }
    void set(Square s, Piece p) { set(s.row, s.col, p); }
    void addPawnMoves(Square from, Colour c, std::vector<Move>& out) const;
    void addStepMoves(Square from, Colour c, const int (*dirs)[2], int count,
                      bool sliding, std::vector<Move>& out) const;
    void addCastlingMoves(Colour c, std::vector<Move>& out) const;
    std::vector<Move> pseudoMoves(Colour c) const;

    std::array<Piece, kSquares> m_cells {};
    Colour m_toMove = Colour::White;
    // [colour][0] is the king side, [1] the queen side.
    bool m_castle[2][2] { { true, true }, { true, true } };
    Square m_ep { -1, -1 };
    int m_halfmove = 0;
    int m_fullmove = 1;
};

enum class Result { Playing, WhiteWins, BlackWins, Draw };

enum class DrawReason { None, Stalemate, FiftyMove, Repetition, InsufficientMaterial };

// A board plus the history a draw claim needs. The search works on Board
// alone, so the position list never gets copied a hundred thousand times.
class ChessGame
{
public:
    ChessGame() { reset(); }

    void reset();
    // Starts a game from a given position, with an empty history. Used to set
    // up endings that would take fifty moves to reach by playing.
    bool setFromFen(const std::string& fen);

    const Board& board() const { return m_board; }
    std::vector<Move> legalMoves() const { return m_board.legalMoves(); }
    Colour toMove() const { return m_board.toMove(); }

    void play(const Move& m);
    // Takes back one ply. False when there is nothing to take back.
    bool undo();
    bool canUndo() const { return !m_past.empty(); }

    Result result() const;
    DrawReason drawReason() const;
    bool isOver() const { return result() != Result::Playing; }

    // How many times the position now on the board has occurred.
    int repetitionCount() const;

    const std::vector<Move>& history() const { return m_played; }

private:
    Board m_board;
    std::vector<Board> m_past;
    std::vector<Move> m_played;
    std::vector<std::string> m_keys;
};

} // namespace chess
