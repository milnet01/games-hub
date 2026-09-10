#include "donatedialog.h"

#include "donate.h"
#include "funding.h"
#include "legibility.h"
#include "legiblefont.h"

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

} // namespace

DonateDialog::DonateDialog(bool offerToStopAsking, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Support Games"));
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
    auto* heading = new QLabel(tr("Games is free, and stays free"), this);
    heading->setObjectName(QStringLiteral("donateHeading"));
    QFont hf = heading->font();
    growByPoints(hf, 4.0);
    hf.setBold(true);
    heading->setFont(hf);
    outer->addWidget(heading);

    auto* blurb = new QLabel(
        tr(
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
            tr("Open %1 in your browser").arg(QString::fromLatin1(link.label)), this);
        button->setToolTip(url);
        connect(button, &QPushButton::clicked, this, [url] {
            // These are generated from FUNDING.yml at configure time, so the
            // scheme is not in doubt today. The check is here because openUrl
            // hands whatever it is given to the desktop, which will launch a
            // handler for a scheme that is not a web page at all -- and the one
            // thing this dialog promises is that a button opens a browser.
            const QUrl target(url);
            if (target.scheme() != QLatin1String("http")         // untranslated: a URL scheme
                && target.scheme() != QLatin1String("https")) {  // untranslated: a URL scheme
                qWarning("Refusing to open \"%s\": not a web address.", qPrintable(url));
                return;
            }
            QDesktopServices::openUrl(target);
        });
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
        m_keepAsking = new QCheckBox(tr("Keep asking me now and then"), this);
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
