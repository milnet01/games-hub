#include "update/updatedialog.h"

#include "legibility.h"
#include "legiblefont.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace update {

namespace {

// Large play grows every dialog here by the amount Preferences grows itself,
// read once so the dialog does not re-lay itself out under the pointer.
void growForLargePlay(QWidget* w)
{
    if (!Legibility::instance().enabled())
        return;
    QFont f = w->font();
    growByPoints(f, 3.0);
    w->setFont(f);
}

} // namespace

UpdateDialog::UpdateDialog(const Offer& offer, Install install, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("A new version of Games"));
    setObjectName(QStringLiteral("updateDialog"));
    growForLargePlay(this);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(22, 20, 22, 18);
    outer->setSpacing(12);

    auto* heading = new QLabel(tr("Games %1 is ready. You have %2.")
                                   .arg(offer.newest.version.toString(),
                                        QStringLiteral(GAMESHUB_VERSION)),
                               this);
    QFont hf = heading->font();
    growByPoints(hf, 4.0);
    hf.setBold(true);
    heading->setFont(hf);
    heading->setWordWrap(true);
    outer->addWidget(heading);

    auto* intro = new QLabel(offer.missed.size() > 1
                                 ? tr("What changed in each version since yours:")
                                 : tr("What changed:"),
                             this);
    outer->addWidget(intro);

    // Read-only and with links inert: the notes are the project's own
    // changelog, but nothing in them should navigate anywhere from here.
    auto* notes = new QTextBrowser(this);
    notes->setObjectName(QStringLiteral("updateNotes"));
    notes->setOpenLinks(false);
    notes->setOpenExternalLinks(false);
    notes->setMarkdown(notesMarkdown(offer.missed));
    notes->setMinimumHeight(fontMetrics().height() * 14);
    outer->addWidget(notes, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* now = buttons->addButton(install == Install::AppImage ? tr("Update now")
                                                                : tr("Open the download page"),
                                   QDialogButtonBox::AcceptRole);
    now->setObjectName(QStringLiteral("updateNow"));
    auto* later = buttons->addButton(tr("Later"), QDialogButtonBox::RejectRole);
    later->setObjectName(QStringLiteral("updateLater"));
    auto* skip = buttons->addButton(tr("Skip this version"), QDialogButtonBox::DestructiveRole);
    skip->setObjectName(QStringLiteral("updateSkip"));
    now->setDefault(true);
    connect(now, &QPushButton::clicked, this, [this] {
        m_choice = Updater::Choice::Install;
        accept();
    });
    connect(later, &QPushButton::clicked, this, [this] {
        m_choice = Updater::Choice::Later;
        reject();
    });
    connect(skip, &QPushButton::clicked, this, [this] {
        m_choice = Updater::Choice::Skip;
        accept();
    });
    outer->addWidget(buttons);

    setMinimumWidth(Legibility::instance().enabled() ? 680 : 560);
}

bool askToCheckDaily(QWidget* parent)
{
    QMessageBox box(QMessageBox::Question, QObject::tr("Check for new versions?"),
                    QObject::tr("Should Games check once a day whether a new version is out?"),
                    QMessageBox::Yes | QMessageBox::No, parent);
    box.setObjectName(QStringLiteral("updateConsent"));
    box.setInformativeText(
        QObject::tr("It asks GitHub, where Games is published, and sends nothing about you. "
                    "You can change this later in Settings, Preferences, and check by hand "
                    "from the Help menu either way."));
    box.setDefaultButton(QMessageBox::Yes);
    box.setEscapeButton(QMessageBox::No);
    growForLargePlay(&box);
    return box.exec() == QMessageBox::Yes;
}

void tellPlayer(QWidget* parent, const QString& text, const QUrl& page)
{
    QMessageBox box(QMessageBox::Information, QObject::tr("Updates"), text, QMessageBox::Ok,
                    parent);
    QPushButton* open = nullptr;
    if (isGitHubPage(page))
        open = box.addButton(QObject::tr("Open the download page"), QMessageBox::ActionRole);
    growForLargePlay(&box);
    box.exec();
    if (open != nullptr && box.clickedButton() == open)
        QDesktopServices::openUrl(page);
}

} // namespace update
