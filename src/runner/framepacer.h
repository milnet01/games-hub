#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>

#include <functional>

class QTimer;
class QWidget;
class QWindow;

// The clock both Ruin Runner views run on (GHUB-0261). It calls `frame` with
// the wall time since the last call, capped at a tenth of a second, so a late
// frame never slows the run.
//
// Under Wayland it follows the screen: a frame starts when the compositor says
// the last one has been shown, so each painted frame is one the screen shows.
// A fixed 16 ms timer ran at 62.5 a second against a 60 Hz screen, and the two
// drifted apart into a visible hitch several times a second. Elsewhere, with
// no such signal to follow, the timer stays.
class FramePacer : public QObject
{
public:
    FramePacer(QWidget* view, std::function<void(double)> frame);

    void start();
    void stop();
    bool running() const { return m_running; }

    // The view calls this at the end of each paint: under Wayland it asks for
    // the next frame, which arrives once the screen has shown this one.
    void painted();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void step();
    // Under Wayland, finds the view's window and watches it; false where
    // there is no window yet, or nothing to follow.
    bool followScreen();

    QWidget* m_view;
    std::function<void(double)> m_frame;
    QTimer* m_timer;
    QElapsedTimer m_clock;
    QPointer<QWindow> m_window;
    bool m_followScreen;
    bool m_running = false;
};
