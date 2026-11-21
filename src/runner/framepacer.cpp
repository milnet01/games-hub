#include "runner/framepacer.h"

#include <QEvent>
#include <QGuiApplication>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <algorithm>

namespace {
constexpr int kFrameMs = 16;
// The most one frame may move the run on. A longer stall slows the game
// instead, so a hitch never carries the runner into something unseen.
constexpr double kLongestFrame = 0.1;
} // namespace

FramePacer::FramePacer(QWidget* view, std::function<void(double)> frame)
    : QObject(view)
    , m_view(view)
    , m_frame(std::move(frame))
    , m_timer(new QTimer(this))
    , m_followScreen(QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) // untranslated: Qt's platform id
{
    m_timer->setInterval(kFrameMs);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &FramePacer::step);
}

void FramePacer::start()
{
    m_running = true;
    m_clock.start();
    if (followScreen())
        m_window->requestUpdate();
    else
        m_timer->start();
}

void FramePacer::stop()
{
    m_running = false;
    m_timer->stop();
}

void FramePacer::painted()
{
    if (!m_running)
        return;
    // A game is started before the hub's window exists, so the timer runs
    // its first frames and hands over here, at the first paint.
    if (m_timer->isActive() && followScreen())
        m_timer->stop();
    if (!m_timer->isActive() && m_window)
        m_window->requestUpdate();
}

bool FramePacer::followScreen()
{
    QWindow* window = m_followScreen ? m_view->window()->windowHandle() : nullptr;
    if (window == nullptr)
        return false;
    if (window != m_window) {
        if (m_window)
            m_window->removeEventFilter(this);
        m_window = window;
        m_window->installEventFilter(this);
    }
    return true;
}

bool FramePacer::eventFilter(QObject* watched, QEvent* event)
{
    // Only the window's own update request moves the run: the view's update()
    // posts its paint straight to the widget, so this sees one call a frame.
    if (watched == m_window && event->type() == QEvent::UpdateRequest && m_running && !m_timer->isActive())
        step();
    return QObject::eventFilter(watched, event);
}

void FramePacer::step()
{
    m_frame(std::min(kLongestFrame, double(m_clock.restart()) / 1000.0));
}
