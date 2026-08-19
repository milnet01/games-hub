#pragma once

#include <QDialog>

class QCheckBox;

// The one place the app asks for money, reached two ways: Help → Support this
// project, and the every-150th-launch prompt.
//
// It explains itself before it asks, and every link says in words what it is
// and where it will take you — the owner reads slowly, and a row of bare
// platform icons is exactly the thing that cannot be read slowly. It opens
// nothing on its own: a browser only ever opens because a named button was
// pressed.
class DonateDialog : public QDialog
{
    Q_OBJECT

public:
    // `offerToStopAsking` adds the "don't ask me again" switch. It is on for
    // the launch prompt, which the player did not ask for, and off for the
    // Help menu entry, which they did — there is nothing to silence when they
    // opened it themselves.
    explicit DonateDialog(bool offerToStopAsking, QWidget* parent = nullptr);

private:
    QCheckBox* m_keepAsking = nullptr;
};
