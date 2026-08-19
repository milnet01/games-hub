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
        f.setPointSizeF(f.pointSizeF() + 3.0);
        setFont(f);
    }

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(22, 20, 22, 18);
    outer->setSpacing(12);

    // What it is, before what it wants.
    auto* heading = new QLabel(QStringLiteral("Games is free, and stays free"), this);
    QFont hf = heading->font();
    hf.setPointSizeF(hf.pointSizeF() + 4);
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
        QPalette pal = address->palette();
        pal.setColor(QPalette::WindowText, pal.color(QPalette::Dark));
        address->setPalette(pal);
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
