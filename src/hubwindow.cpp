#include "hubwindow.h"

#include "gameview.h"
#include "hearts/heartsview.h"
#include "klondike/klondikeview.h"
#include "minesweeper/minesweeperview.h"
#include "pinball/pinballview.h"
#include "reversi/reversiview.h"
#include "spider/spiderview.h"

#include <QApplication>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
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
        { QStringLiteral("Reversi"), QStringLiteral("Flip the board"), reversiTile,
          [] { return new ReversiView; } },
        { QStringLiteral("Minesweeper"), QStringLiteral("Clear the field"), minesweeperTile,
          [] { return new MinesweeperView; } },
        { QStringLiteral("Solitaire"), QStringLiteral("Klondike"), klondikeTile,
          [] { return new KlondikeView; } },
        { QStringLiteral("Spider"), QStringLiteral("Solitaire, harder"), spiderTile,
          [] { return new SpiderView; } },
        { QStringLiteral("Hearts"), QStringLiteral("Avoid the tricks"), heartsTile,
          [] { return new HeartsView; } },
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
        connect(tile, &QPushButton::clicked, this, [this, i] { openGame(i); });
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
    connect(m_backAction, &QAction::triggered, this, &HubWindow::showMenu);
    m_toolBar->addAction(m_backAction);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status, 1);
}

void HubWindow::showMenu()
{
    setGameActions(nullptr);
    m_backAction->setVisible(false);
    m_stack->setCurrentWidget(m_menuPage);
    setWindowTitle(QStringLiteral("Games"));
    m_status->setText(QStringLiteral("Six games. Pick one."));
}

void HubWindow::openGame(int index)
{
    Entry& e = m_entries[index];
    if (!e.view) {
        e.view = e.create();
        connect(e.view, &GameView::statusChanged, m_status, &QLabel::setText);
        e.pageIndex = m_stack->addWidget(e.view);
    }

    setGameActions(e.view);
    m_backAction->setVisible(true);
    m_stack->setCurrentIndex(e.pageIndex);
    setWindowTitle(e.name + QStringLiteral(" — Games"));
    e.view->activate();
    e.view->setFocus();
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
        m_toolBar->addAction(a);
}
