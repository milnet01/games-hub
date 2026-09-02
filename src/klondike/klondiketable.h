#pragma once

#include "dealseed.h"

#include "cards/card.h"

#include <array>
#include <deque>
#include <random>
#include <vector>

// Klondike, the patience everyone means by "Solitaire": seven columns, four
// foundations, and a stock you turn one or three at a time.
//
// The rules only — no widget, no drag geometry, no painting. Extracted so
// gameshub_selftest can reach them (GHUB-0066); Klondike was named once in
// that file and its rules not at all.
class KlondikeTable
{
public:
    static constexpr int kColumns = 7;
    static constexpr int kFoundations = 4;
    static constexpr int kPackSize = 52;

    enum class PileKind { Stock, Waste, Foundation, Tableau };

    // What a move is worth, kept together so the scoring can be read in one
    // place rather than found in four.
    static constexpr int kScoreToFoundation = 10;
    static constexpr int kScoreTableauMove = 5;
    static constexpr int kScoreTurnUp = 5;

    KlondikeTable() { deal(); }

    void deal();

    const std::vector<Card>& stock() const { return m_stock; }
    const std::vector<Card>& waste() const { return m_waste; }
    const std::array<std::vector<Card>, kFoundations>& foundations() const
    {
        return m_foundations;
    }
    const std::array<std::vector<Card>, kColumns>& tableau() const { return m_tableau; }
    const std::vector<Card>& pile(PileKind kind, int index) const;
    int score() const { return m_score; }
    bool won() const;

    int drawCount() const { return m_drawCount; }
    void setDrawCount(int count);

    bool canStackOnTableau(const Card& moving, int column) const;
    bool canPlaceOnFoundation(const Card& moving, int foundation) const;

    // Whether the player may take hold here: a face-up card, and in a column
    // every card below it face up too.
    bool canLift(PileKind kind, int index, int from) const;

    // Picks a run up off a pile and HOLDS it, so the table knows where it came
    // from and can turn over whatever it was covering when it lands. Returns
    // the cards for the view to draw; empty if the hold is not allowed.
    //
    // The undo snapshot is banked HERE, before the cards leave their pile.
    // Klondike snapshotted at DROP time, by which point the drag had already
    // erased them, so undoing a finished move restored a table they had never
    // been on and they were gone -- the same defect measured and fixed in
    // FreeCell as GHUB-0126.
    std::vector<Card> lift(PileKind kind, int index, int from);
    bool holding() const { return !m_held.empty(); }
    const std::vector<Card>& held() const { return m_held; }

    // Puts the held run back where it came from and drops the snapshot lift()
    // banked: nothing happened, so there is nothing to undo.
    void putBack();

    // Place the held run. False leaves it held, for putBack().
    bool dropOnFoundation(int foundation);
    bool dropOnTableau(int column);

    // Sends the top card of a pile to whichever foundation will take it.
    bool sendToFoundation(PileKind kind, int index);
    // One card of the auto-finish sweep. False when nothing more will go.
    bool autoFinishStep();

    // Turns drawCount cards to the waste, or turns the waste back under the
    // stock when it is empty.
    void dealFromStock();

    bool canUndo() const { return !m_history.empty(); }
    void undo();

    // Adopts a position from a save. Klondike never takes a card out of play,
    // so the whole pack must come back. False leaves this object alone.
    bool restore(const std::vector<Card>& stock, const std::vector<Card>& waste,
                 const std::array<std::vector<Card>, kFoundations>& foundations,
                 const std::array<std::vector<Card>, kColumns>& tableau, int drawCount,
                 int score);

private:
    struct Snapshot {
        std::vector<Card> stock;
        std::vector<Card> waste;
        std::array<std::vector<Card>, kFoundations> foundations;
        std::array<std::vector<Card>, kColumns> tableau;
        int score = 0;
    };

    std::vector<Card>& pileAt(PileKind kind, int index);
    void pushUndo();
    void dropUndo();
    // Turns up whatever the held run was covering, once it has landed.
    void revealSource();

    std::vector<Card> m_stock;
    std::vector<Card> m_waste;
    std::array<std::vector<Card>, kFoundations> m_foundations;
    std::array<std::vector<Card>, kColumns> m_tableau;

    std::vector<Card> m_held;
    PileKind m_heldFrom = PileKind::Tableau;
    int m_heldPile = 0;

    std::deque<Snapshot> m_history;
    std::mt19937 m_rng { dealSeed() };
    int m_drawCount = 1;
    int m_score = 0;
};
