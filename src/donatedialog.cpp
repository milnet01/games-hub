#include "donatedialog.h"

#include "donate.h"
#include "funding.h"
#include "legibility.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace {

// QFont carries EITHER a point size or a pixel size, and answers -1 for the one
// it is not using. Adding points to a pixel-sized font therefore produced a
// 2.0pt dialog -- microscopic, and worst of all in the very branch meant to
// make the text bigger. Grow whichever unit is actually in force.
void growByPoints(QFont& f, double points)
{
    if (f.pointSizeF() > 0.0) {
        f.setPointSizeF(f.pointSizeF() + points);
        return;
    }
    if (f.pixelSize() > 0) {
        // A point is 1/72 inch against Qt's 96 logical DPI, so roughly 4/3 of a
        // pixel. Exactness does not matter here; not shrinking the text does.
        f.setPixelSize(f.pixelSize() + int(std::lround(points * 4.0 / 3.0)));
    }
}

} // namespace

DonateDialog::DonateDialog(bool offerToStopAsking, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Support Games"));
    setObjectName(QStringLiteral("donateDialog"));

    // The legibility switch reaches here like anywhere else. A dialog lives for
    // a few seconds, so it reads the setting once at construction rather than
    // subscribing: nobody moves the switch while this is on screen, and the
    // next one built after they do gets the new size.
    const bool large = Legibility::instance().enabled();
    if (large) {
        QFont f = font();
        growByPoints(f, 3.0);
        setFont(f);
    }

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(22, 20, 22, 18);
    outer->setSpacing(12);

    // What it is, before what it wants.
    auto* heading = new QLabel(QStringLiteral("Games is free, and stays free"), this);
    heading->setObjectName(QStringLiteral("donateHeading"));
    QFont hf = heading->font();
    growByPoints(hf, 4.0);
    hf.setBold(true);
    heading->setFont(hf);
    outer->addWidget(heading);

    auto* blurb = new QLabel(
        QStringLiteral(
            "There is nothing to buy here and nothing is locked away. If you enjoy "
            "the collection and would like to help it keep growing, any of the three "
            "places below will take a contribution.\n\n"
            "Each button opens that page in your web browser. Nothing is sent from "
            "this program."),
        this);
    blurb->setWordWrap(true);
    outer->addWidget(blurb);

    for (const funding::Link& link : funding::kLinks) {
        const QString url = QString::fromLatin1(link.url);

        auto* button = new QPushButton(
            QStringLiteral("Open %1 in your browser").arg(QString::fromLatin1(link.label)), this);
        button->setToolTip(url);
        connect(button, &QPushButton::clicked, this,
                [url] { QDesktopServices::openUrl(QUrl(url)); });
        outer->addWidget(button);

        // The address in full underneath, so the destination is readable before
        // the browser opens rather than after.
        auto* address = new QLabel(url, this);
        address->setTextInteractionFlags(Qt::TextSelectableByMouse);
        address->setWordWrap(true);
        // Not dimmed. It used to be painted in QPalette::Dark, which is a
        // 3D-shadow role with no guaranteed contrast against the window -- on
        // the one address a partially sighted reader is being asked to read.
        // The button above it already carries the hierarchy this was for.
        outer->addWidget(address);
    }

    if (offerToStopAsking) {
        outer->addSpacing(6);
        m_keepAsking = new QCheckBox(QStringLiteral("Keep asking me now and then"), this);
        m_keepAsking->setObjectName(QStringLiteral("donateKeepAsking"));
        m_keepAsking->setChecked(donate::asksEnabled());
        // Stored as it is toggled rather than on accept, so closing the dialog
        // with the window button honours the choice as well as the Close button.
        connect(m_keepAsking, &QCheckBox::toggled, this,
                [](bool on) { donate::setAsksEnabled(on); });
        outer->addWidget(m_keepAsking);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    setMinimumWidth(large ? 560 : 460);
}
