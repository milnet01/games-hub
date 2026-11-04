#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QSlider;

// The one place that shows every app-wide setting (GHUB-0068): sound, its
// volume, large play, whether the app asks for support now and then, and --
// in a download that can update itself -- the daily update check (GHUB-0043).
// Each game's own rules stay inside that game, where they are wanted.
//
// Every change applies the moment it is made, like the toolbar switches it
// sits beside, so there is no OK to forget and nothing to undo. The toolbar
// switches stay too: this is a place to see everything, not a replacement
// for the one-click ones.
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

private:
    void showVolume(int percent);

    QCheckBox* m_sound = nullptr;
    QSlider* m_volume = nullptr;
    QLabel* m_volumeText = nullptr;
};
