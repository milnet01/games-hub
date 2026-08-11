#include "hubwindow.h"

#include "gameview.h"
#include "sound.h"
#include "chess/chessart.h"
#include "chess/chessview.h"
#include "draughts/draughtsview.h"
#include "freecell/freecellview.h"
#include "canasta/canastaview.h"
#include "hearts/heartsview.h"
#include "klondike/klondikeview.h"
#include "minesweeper/minesweeperview.h"
#include "pinball/pinballview.h"
#include "pyramid/pyramidview.h"
#include "reversi/reversiview.h"
#include "spider/spiderview.h"
#include "snake/snakeview.h"
#include "sudoku/sudokuview.h"
#include "twenty48/twenty48view.h"

#include <QApplication>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyleOption>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

constexpr QColor kTileBase { 0x2f, 0x35, 0x3b };
constexpr QColor kTileHover { 0x3c, 0x45, 0x4d };

// A tile button that paints its own miniature of the game it opens.
class GameTile : public QPushButton
{
public:
    GameTile(const QString& name, const QString& blurb,
             std::function<void(QPainter&, const QRectF&)> art, QWidget* parent)
        : QPushButton(parent)
        , m_name(name)
        , m_blurb(blurb)
        , m_art(std::move(art))
    {
        setMinimumSize(190, 190);
        setCursor(Qt::PointingHandCursor);
        setFlat(true);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = rect().adjusted(4, 4, -4, -4);
        QPainterPath card;
        card.addRoundedRect(r, 12, 12);
        p.fillPath(card, underMouse() ? kTileHover : kTileBase);
        if (hasFocus() || underMouse()) {
            p.setPen(QPen(QColor(0x5a, 0xa8, 0x7a), 2));
            p.drawPath(card);
        }

        // Square art area on top, captions underneath.
        const double artSide = std::min(r.width() - 32, r.height() - 74);
        const QRectF art((r.center().x() - artSide / 2), r.top() + 14, artSide, artSide);
        p.save();
        m_art(p, art);
        p.restore();

        QFont title = font();
        title.setBold(true);
        title.setPointSizeF(title.pointSizeF() + 1.0);
        p.setFont(title);
        p.setPen(QColor(0xf0, 0xf2, 0xf4));
        const QRectF nameRect(r.left(), art.bottom() + 8, r.width(), 22);
        p.drawText(nameRect, Qt::AlignHCenter | Qt::AlignVCenter, m_name);

        QFont sub = font();
        sub.setPointSizeF(sub.pointSizeF() - 1.0);
        p.setFont(sub);
        p.setPen(QColor(0x9e, 0xa8, 0xb2));
        const QRectF blurbRect(r.left() + 8, nameRect.bottom(), r.width() - 16, 20);
        p.drawText(blurbRect, Qt::AlignHCenter | Qt::AlignTop, m_blurb);
    }

    void enterEvent(QEnterEvent*) override { update(); }
    void leaveEvent(QEvent*) override { update(); }

private:
    QString m_name;
    QString m_blurb;
    std::function<void(QPainter&, const QRectF&)> m_art;
};

// ---------------------------------------------------------------------------
// Tile miniatures
// ---------------------------------------------------------------------------

void drawFelt(QPainter& p, const QRectF& r, const QColor& top, const QColor& bottom)
{
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, top);
    g.setColorAt(1, bottom);
    QPainterPath path;
    path.addRoundedRect(r, 8, 8);
    p.fillPath(path, g);
}

void reversiTile(QPainter& p, const QRectF& r)
{
    drawFelt(p, r, QColor(0x2a, 0x91, 0x60), QColor(0x14, 0x60, 0x3c));
    const double cell = r.width() / 4.0;
    p.setPen(QPen(QColor(0x0d, 0x3d, 0x27), 1));
    for (int i = 1; i < 4; ++i) {
        p.drawLine(QPointF(r.left() + i * cell, r.top()), QPointF(r.left() + i * cell, r.bottom()));
        p.drawLine(QPointF(r.left(), r.top() + i * cell), QPointF(r.right(), r.top() + i * cell));
    }
    p.setPen(Qt::NoPen);
    const double rad = cell * 0.36;
    const bool white[4] = { true, false, false, true };
    int k = 0;
    for (int row = 1; row <= 2; ++row)
        for (int col = 1; col <= 2; ++col, ++k) {
            p.setBrush(white[k] ? QColor(0xf4, 0xf3, 0xee) : QColor(0x22, 0x24, 0x26));
            p.drawEllipse(QPointF(r.left() + col * cell, r.top() + row * cell), rad, rad);
        }
}

void chessTile(QPainter& p, const QRectF& r)
{
    // A corner of the board with the two pieces that read fastest at this size.
    const double cell = r.width() / 4.0;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            p.fillRect(QRectF(r.left() + col * cell, r.top() + row * cell, cell, cell),
                       ((row + col) % 2) ? ChessArt::kDarkSquare : ChessArt::kLightSquare);

    ChessArt::paintPiece(p, QRectF(r.left() + cell * 0.55, r.top() + cell * 1.55, cell, cell),
                         chess::PieceType::King, chess::Colour::White);
    ChessArt::paintPiece(p, QRectF(r.left() + cell * 2.45, r.top() + cell * 0.55, cell, cell),
                         chess::PieceType::Knight, chess::Colour::Black);
}

void draughtsTile(QPainter& p, const QRectF& r)
{
    // A corner of a chequerboard with two counters on it.
    const double cell = r.width() / 4.0;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            p.fillRect(QRectF(r.left() + col * cell, r.top() + row * cell, cell, cell),
                       ((row + col) % 2) ? QColor(0x6b, 0x46, 0x2c) : QColor(0xd9, 0xbe, 0x96));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xb5, 0x3a, 0x30));
    p.drawEllipse(QPointF(r.left() + cell * 1.5, r.top() + cell * 2.5), cell * 0.34, cell * 0.34);
    p.setBrush(QColor(0xef, 0xe9, 0xdd));
    p.drawEllipse(QPointF(r.left() + cell * 2.5, r.top() + cell * 1.5), cell * 0.34, cell * 0.34);
}

void minesweeperTile(QPainter& p, const QRectF& r)
{
    QPainterPath path;
    path.addRoundedRect(r, 8, 8);
    p.fillPath(path, QColor(0x4a, 0x51, 0x58));

    const double cell = r.width() / 4.0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            const QRectF c(r.left() + col * cell + 1, r.top() + row * cell + 1, cell - 2, cell - 2);
            const bool dug = (row + col) % 3 == 0;
            p.fillRect(c, dug ? QColor(0x33, 0x38, 0x3d) : QColor(0x6b, 0x74, 0x7d));
            if (!dug) {
                p.setPen(QPen(QColor(0x8a, 0x95, 0x9f), 1));
                p.drawLine(c.topLeft(), c.topRight());
                p.drawLine(c.topLeft(), c.bottomLeft());
            }
        }
    }
    // One mine, centre-ish, so the tile is unmistakable.
    const QPointF mine(r.left() + cell * 2.5, r.top() + cell * 1.5);
    const double rad = cell * 0.3;
    p.setPen(QPen(QColor(0x1a, 0x1c, 0x1e), 2));
    for (int i = 0; i < 4; ++i) {
        const double a = i * M_PI / 4;
        p.drawLine(mine - QPointF(std::cos(a), std::sin(a)) * rad * 1.6,
                   mine + QPointF(std::cos(a), std::sin(a)) * rad * 1.6);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1a, 0x1c, 0x1e));
    p.drawEllipse(mine, rad, rad);
}

// Shared by the two solitaires and Hearts: a small fan of cards.
void cardFan(QPainter& p, const QRectF& r, const QList<QPair<QString, bool>>& faces, double spread)
{
    drawFelt(p, r, QColor(0x1e, 0x6b, 0x46), QColor(0x11, 0x4a, 0x30));

    const double w = r.width() * 0.34;
    const double h = w * 1.4;
    const int n = faces.size();
    for (int i = 0; i < n; ++i) {
        p.save();
        p.translate(r.center().x(), r.center().y() + h * 0.12);
        p.rotate((i - (n - 1) / 2.0) * spread);
        const QRectF card(-w / 2, -h / 2, w, h);
        QPainterPath path;
        path.addRoundedRect(card, 3, 3);
        p.fillPath(path, QColor(0xfa, 0xfa, 0xf7));
        p.setPen(QPen(QColor(0x00, 0x00, 0x00, 60), 1));
        p.drawPath(path);

        QFont f = p.font();
        f.setPointSizeF(std::max(6.0, w * 0.42));
        f.setBold(true);
        p.setFont(f);
        p.setPen(faces[i].second ? QColor(0xc0, 0x30, 0x30) : QColor(0x20, 0x20, 0x20));
        p.drawText(card, Qt::AlignCenter, faces[i].first);
        p.restore();
    }
}

void klondikeTile(QPainter& p, const QRectF& r)
{
    cardFan(p, r, { { QStringLiteral("♠"), false }, { QStringLiteral("A"), true },
                    { QStringLiteral("♦"), true } }, 16);
}

void freecellTile(QPainter& p, const QRectF& r)
{
    cardFan(p, r, { { QStringLiteral("A"), false }, { QStringLiteral("2"), true },
                    { QStringLiteral("3"), false } }, 14);
}

void pyramidTile(QPainter& p, const QRectF& r)
{
    drawFelt(p, r, QColor(0x1c, 0x6b, 0x7e), QColor(0x0d, 0x40, 0x4e));
    // A little stack of three rows, which is the game at a glance.
    const double w = r.width() * 0.24;
    const double h = w * 1.4;
    for (int row = 0; row < 3; ++row) {
        for (int i = 0; i <= row; ++i) {
            const double rowWidth = w * 0.56 * row + w;
            const QRectF card(r.center().x() - rowWidth / 2 + i * w * 0.56,
                              r.top() + r.height() * 0.16 + row * h * 0.46, w, h);
            QPainterPath path;
            path.addRoundedRect(card, 2, 2);
            p.fillPath(path, QColor(0xfa, 0xfa, 0xf7));
            p.setPen(QPen(QColor(0, 0, 0, 60), 1));
            p.drawPath(path);
        }
    }
}

void sudokuTile(QPainter& p, const QRectF& r)
{
    p.fillRect(r, QColor(0xf6, 0xf3, 0xe8));
    const double cell = r.width() / 3.0;
    for (int br = 0; br < 3; ++br)
        for (int bc = 0; bc < 3; ++bc)
            if ((br + bc) % 2 == 0)
                p.fillRect(QRectF(r.left() + bc * cell, r.top() + br * cell, cell, cell),
                           QColor(0xe9, 0xe4, 0xd4));
    p.setPen(QPen(QColor(0x3c, 0x38, 0x30), 2));
    for (int i = 0; i <= 3; ++i) {
        p.drawLine(QPointF(r.left() + i * cell, r.top()), QPointF(r.left() + i * cell, r.bottom()));
        p.drawLine(QPointF(r.left(), r.top() + i * cell), QPointF(r.right(), r.top() + i * cell));
    }
    QFont f = p.font();
    f.setBold(true);
    f.setPointSizeF(cell * 0.42);
    p.setFont(f);
    const char* digits[9] = { "5", "", "9", "", "7", "", "2", "", "6" };
    for (int i = 0; i < 9; ++i) {
        if (!*digits[i])
            continue;
        p.setPen(i % 2 ? QColor(0x1f, 0x6f, 0xb2) : QColor(0x22, 0x26, 0x2b));
        p.drawText(QRectF(r.left() + (i % 3) * cell, r.top() + (i / 3) * cell, cell, cell),
                   Qt::AlignCenter, QString::fromUtf8(digits[i]));
    }
}

void spiderTile(QPainter& p, const QRectF& r)
{
    cardFan(p, r, { { QStringLiteral("K"), false }, { QStringLiteral("Q"), false },
                    { QStringLiteral("J"), false }, { QStringLiteral("10"), false } }, 11);
}

void heartsTile(QPainter& p, const QRectF& r)
{
    // The heart goes last so it lands on top of the fan — it is the tile's
    // whole identity.
    cardFan(p, r, { { QStringLiteral("♠"), false }, { QStringLiteral("Q"), false },
                    { QStringLiteral("♥"), true } }, 15);
}

void canastaTile(QPainter& p, const QRectF& r)
{
    // A meld with a joker in it: the one card no other game in the hub has.
    cardFan(p, r, { { QStringLiteral("K"), false }, { QStringLiteral("K"), true },
                    { QStringLiteral("★"), false } }, 14);
}

void snakeTile(QPainter& p, const QRectF& r)
{
    const double cell = r.width() / 5.0;
    for (int x = 0; x < 5; ++x)
        for (int y = 0; y < 5; ++y)
            p.fillRect(QRectF(r.left() + x * cell, r.top() + y * cell, cell, cell),
                       ((x + y) % 2) ? QColor(0x18, 0x38, 0x28) : QColor(0x14, 0x30, 0x22));
    p.setPen(Qt::NoPen);
    const QPoint body[5] = { { 1, 3 }, { 1, 2 }, { 2, 2 }, { 3, 2 }, { 3, 1 } };
    for (int i = 0; i < 5; ++i) {
        p.setBrush(QColor::fromHsvF(0.33, 0.55, 0.85 - i * 0.07));
        p.drawRoundedRect(QRectF(r.left() + body[i].x() * cell + cell * 0.08,
                                 r.top() + body[i].y() * cell + cell * 0.08,
                                 cell * 0.84, cell * 0.84),
                          cell * 0.26, cell * 0.26);
    }
    p.setBrush(QColor(0xe8, 0x51, 0x4f));
    p.drawEllipse(QPointF(r.left() + cell * 3.5, r.top() + cell * 3.5), cell * 0.28, cell * 0.28);
}

void twenty48Tile(QPainter& p, const QRectF& r)
{
    QPainterPath path;
    path.addRoundedRect(r, 8, 8);
    p.fillPath(path, QColor(0xbb, 0xad, 0xa0));

    const double gap = r.width() * 0.05;
    const double cell = (r.width() - gap * 3) / 2.0;
    const int values[4] = { 2, 4, 8, 16 };
    const QColor colours[4] = { QColor(0xee, 0xe4, 0xda), QColor(0xed, 0xe0, 0xc8),
                                QColor(0xf2, 0xb1, 0x79), QColor(0xf5, 0x95, 0x63) };
    QFont f = p.font();
    f.setBold(true);
    f.setPointSizeF(cell * 0.36);
    p.setFont(f);
    for (int i = 0; i < 4; ++i) {
        const QRectF box(r.left() + gap + (i % 2) * (cell + gap),
                         r.top() + gap + (i / 2) * (cell + gap), cell, cell);
        QPainterPath tile;
        tile.addRoundedRect(box, 5, 5);
        p.fillPath(tile, colours[i]);
        p.setPen(values[i] <= 4 ? QColor(0x77, 0x6e, 0x65) : QColor(0xf9, 0xf6, 0xf2));
        p.drawText(box, Qt::AlignCenter, QString::number(values[i]));
    }
}

void pinballTile(QPainter& p, const QRectF& r)
{
    QPainterPath path;
    path.addRoundedRect(r, 8, 8);
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, QColor(0x24, 0x2c, 0x50));
    g.setColorAt(1, QColor(0x12, 0x16, 0x2c));
    p.fillPath(path, g);

    // Two bumpers and a ball, with the flippers hinted at the bottom.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xe8, 0x51, 0x4f));
    p.drawEllipse(QPointF(r.left() + r.width() * 0.34, r.top() + r.height() * 0.30), r.width() * 0.10, r.width() * 0.10);
    p.setBrush(QColor(0xf2, 0xb1, 0x3c));
    p.drawEllipse(QPointF(r.left() + r.width() * 0.64, r.top() + r.height() * 0.42), r.width() * 0.09, r.width() * 0.09);

    p.setPen(QPen(QColor(0xcf, 0xd6, 0xe0), std::max(2.0, r.width() * 0.05), Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(r.left() + r.width() * 0.22, r.bottom() - r.height() * 0.22),
               QPointF(r.left() + r.width() * 0.44, r.bottom() - r.height() * 0.13));
    p.drawLine(QPointF(r.right() - r.width() * 0.22, r.bottom() - r.height() * 0.22),
               QPointF(r.right() - r.width() * 0.44, r.bottom() - r.height() * 0.13));

    p.setPen(Qt::NoPen);
    QRadialGradient ball(QPointF(r.left() + r.width() * 0.5, r.top() + r.height() * 0.62), r.width() * 0.12);
    ball.setColorAt(0, QColor(0xff, 0xff, 0xff));
    ball.setColorAt(1, QColor(0x88, 0x90, 0x9a));
    p.setBrush(ball);
    p.drawEllipse(QPointF(r.left() + r.width() * 0.5, r.top() + r.height() * 0.62), r.width() * 0.09, r.width() * 0.09);
}

} // namespace

HubWindow::HubWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Games"));
    buildEntries();
    buildChrome();
    showMenu();
}

void HubWindow::buildEntries()
{
    m_entries = {
        { QStringLiteral("Chess"), QStringLiteral("The full game"), chessTile,
          [] { return new ChessView; } },
        { QStringLiteral("Reversi"), QStringLiteral("Flip the board"), reversiTile,
          [] { return new ReversiView; } },
        { QStringLiteral("Draughts"), QStringLiteral("Checkers, with kings"), draughtsTile,
          [] { return new DraughtsView; } },
        { QStringLiteral("Minesweeper"), QStringLiteral("Clear the field"), minesweeperTile,
          [] { return new MinesweeperView; } },
        { QStringLiteral("Solitaire"), QStringLiteral("Klondike"), klondikeTile,
          [] { return new KlondikeView; } },
        { QStringLiteral("Spider"), QStringLiteral("Solitaire, harder"), spiderTile,
          [] { return new SpiderView; } },
        { QStringLiteral("FreeCell"), QStringLiteral("Solitaire, solvable"), freecellTile,
          [] { return new FreeCellView; } },
        { QStringLiteral("Pyramid"), QStringLiteral("Pairs make 13"), pyramidTile,
          [] { return new PyramidView; } },
        { QStringLiteral("Sudoku"), QStringLiteral("Fill the grid"), sudokuTile,
          [] { return new SudokuView; } },
        { QStringLiteral("Hearts"), QStringLiteral("Avoid the tricks"), heartsTile,
          [] { return new HeartsView; } },
        { QStringLiteral("Canasta"), QStringLiteral("Melds and partners"), canastaTile,
          [] { return new CanastaView; } },
        { QStringLiteral("Snake"), QStringLiteral("Eat and grow"), snakeTile,
          [] { return new SnakeView; } },
        { QStringLiteral("2048"), QStringLiteral("Slide and merge"), twenty48Tile,
          [] { return new Twenty48View; } },
        { QStringLiteral("Pinball"), QStringLiteral("Keep it alive"), pinballTile,
          [] { return new PinballView; } },
    };
}

void HubWindow::buildChrome()
{
    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    m_menuPage = new QWidget(this);
    auto* outer = new QVBoxLayout(m_menuPage);
    outer->setContentsMargins(20, 18, 20, 20);

    auto* heading = new QLabel(QStringLiteral("Pick a game"), m_menuPage);
    QFont hf = heading->font();
    hf.setPointSizeF(hf.pointSizeF() + 5);
    hf.setBold(true);
    heading->setFont(hf);
    outer->addWidget(heading);
    outer->addSpacing(10);

    auto* grid = new QGridLayout;
    grid->setSpacing(14);
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry& e = m_entries[i];
        auto* tile = new GameTile(e.name, e.blurb, e.paintTile, m_menuPage);
        connect(tile, &QPushButton::clicked, this, [this, i] {
            Sound::instance().play(Sound::kClick);
            openGame(i);
        });
        grid->addWidget(tile, i / 3, i % 3);
    }
    outer->addLayout(grid);
    outer->addStretch(1);

    m_stack->addWidget(m_menuPage);

    m_toolBar = addToolBar(QStringLiteral("Game"));
    m_toolBar->setMovable(false);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_backAction = new QAction(QStringLiteral("← All Games"), this);
    m_backAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(m_backAction, &QAction::triggered, this, [this] {
        Sound::instance().play(Sound::kBack);
        showMenu();
    });
    m_toolBar->addAction(m_backAction);

    // One sound switch for the whole collection, kept at the far end of the
    // toolbar so it never moves when a game swaps its own actions in.
    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_soundSeparator = m_toolBar->addWidget(spacer);

    m_soundAction = new QAction(QStringLiteral("🔊 Sound"), this);
    m_soundAction->setCheckable(true);
    m_soundAction->setChecked(true);
    m_soundAction->setToolTip(QStringLiteral("Turn game sounds on or off"));
    connect(m_soundAction, &QAction::toggled, this, [this](bool on) {
        Sound::instance().setMuted(!on);
        m_soundAction->setText(on ? QStringLiteral("🔊 Sound") : QStringLiteral("🔇 Muted"));
        if (on)
            Sound::instance().play(Sound::kClick);
    });
    m_toolBar->addAction(m_soundAction);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status, 1);
}

// ---------------------------------------------------------------------------
// Remembering where you were
// ---------------------------------------------------------------------------

namespace {

QString geometryKey(const QString& page)
{
    return QStringLiteral("window/geometry/") + (page.isEmpty() ? QStringLiteral("menu") : page);
}

QString saveKey(const QString& game)
{
    return QStringLiteral("saved/") + game;
}

} // namespace

GameView* HubWindow::currentView() const
{
    for (const Entry& e : m_entries)
        if (e.view != nullptr && e.name == m_page)
            return e.view;
    return nullptr;
}

void HubWindow::rememberPage()
{
    // Nothing to remember until a page has actually been on screen: the first
    // call comes from the constructor, before the window has a size worth
    // keeping, and it would overwrite the one being restored.
    if (!m_geometryReady)
        return;

    // saveGeometry() carries the position, the size and whether the window was
    // maximised, which is why it is used rather than pos() and size().
    QSettings().setValue(geometryKey(m_page), saveGeometry());

    for (const Entry& e : m_entries)
        if (e.view != nullptr && e.name == m_page)
            storeSave(e);
}

void HubWindow::applyPageGeometry(const QString& page)
{
    const QVariant saved = QSettings().value(geometryKey(page));
    if (saved.isValid())
        restoreGeometry(saved.toByteArray());
    else if (!m_geometryReady)
        resize(880, 680); // first run, with nothing remembered for anything
    // Switching to a game that has never been sized keeps the size you are
    // already looking at, rather than jumping to an arbitrary default.
    m_page = page;
    m_geometryReady = true;
}

void HubWindow::storeSave(const Entry& e) const
{
    if (e.view == nullptr)
        return;
    const QByteArray state = e.view->saveState();
    QSettings s;
    // An empty state means the game has nothing worth coming back to — a
    // finished hand, or a game that does not offer saving at all. Clearing
    // rather than keeping stops a stale position outliving the game it came
    // from.
    if (state.isEmpty())
        s.remove(saveKey(e.name));
    else
        s.setValue(saveKey(e.name), state);
}

void HubWindow::closeEvent(QCloseEvent* event)
{
    rememberPage();
    // Every game that has been opened this session, not just the one on screen.
    for (const Entry& e : m_entries)
        storeSave(e);
    QMainWindow::closeEvent(event);
}

void HubWindow::showMenu()
{
    rememberPage();
    if (GameView* leaving = currentView())
        leaving->deactivate();
    setGameActions(nullptr);
    m_backAction->setVisible(false);
    m_stack->setCurrentWidget(m_menuPage);
    setWindowTitle(QStringLiteral("Games"));
    m_status->setText(QStringLiteral("%1 games. Pick one.").arg(m_entries.size()));
    applyPageGeometry(QString());
}

void HubWindow::openGame(int index)
{
    Entry& e = m_entries[index];
    rememberPage();
    if (GameView* leaving = currentView(); leaving != nullptr && leaving != e.view)
        leaving->deactivate();

    bool resumed = false;
    if (!e.view) {
        e.view = e.create();
        // Only the page on screen writes to the status bar. Connecting every
        // game to it let a background game's clock overwrite the line
        // belonging to the game being played.
        GameView* view = e.view;
        connect(view, &GameView::statusChanged, this, [this, view](const QString& text) {
            if (view == currentView())
                m_status->setText(text);
        });
        e.pageIndex = m_stack->addWidget(e.view);

        // Pick the game up where it was left, if it kept anything. Done here
        // rather than in the game's constructor so a game that cannot read an
        // older save simply keeps the fresh one it just dealt.
        const QByteArray state = QSettings().value(saveKey(e.name)).toByteArray();
        if (!state.isEmpty())
            resumed = e.view->restoreState(state);
    }

    setGameActions(e.view);
    m_backAction->setVisible(true);
    m_stack->setCurrentIndex(e.pageIndex);
    setWindowTitle(e.name + QStringLiteral(" — Games"));
    applyPageGeometry(e.name);
    e.view->activate();
    e.view->setFocus();
    if (resumed)
        m_status->setText(QStringLiteral("Carried on from where you left off."));
}

QStringList HubWindow::gameNames() const
{
    QStringList names;
    for (const Entry& e : m_entries)
        names << e.name;
    return names;
}

bool HubWindow::openGameNamed(const QString& name)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].name.compare(name, Qt::CaseInsensitive) == 0) {
            openGame(i);
            return true;
        }
    }
    return false;
}

void HubWindow::setGameActions(GameView* view)
{
    for (QAction* a : m_gameActions)
        m_toolBar->removeAction(a);
    m_gameActions.clear();

    if (!view)
        return;

    m_gameActions = view->gameActions();
    for (QAction* a : m_gameActions)
        m_toolBar->insertAction(m_soundSeparator, a);
}
