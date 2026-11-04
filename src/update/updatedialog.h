#pragma once

#include "update/updater.h"

#include <QDialog>

namespace update {

// The offer (GHUB-0043 § 4.7): the running and the new version, every missed
// release's notes in a scrolling box, and three buttons. A dialog over the
// hub rather than a status-bar line, which is not read during play.
class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    UpdateDialog(const Offer& offer, Install install, QWidget* parent = nullptr);

    Updater::Choice choice() const { return m_choice; }

private:
    Updater::Choice m_choice = Updater::Choice::Later;
};

// The first-run question (§ 4.5). True for yes.
bool askToCheckDaily(QWidget* parent);

// A one-sentence message, with a button opening `page` when it is valid.
void tellPlayer(QWidget* parent, const QString& text, const QUrl& page);

} // namespace update
