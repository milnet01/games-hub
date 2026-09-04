#include "sound.h"

#include <QSettings>
#include <QSoundEffect>
#include <QUrl>

namespace {
// A function for the reason scores.h's own key helpers are: nothing is built
// at static-initialisation time, and QStringLiteral's data is static, so the
// copy this returns costs nothing.
QString mutedKey() { return QStringLiteral("audio/muted"); }
}

Sound& Sound::instance()
{
    static Sound sound;
    return sound;
}

Sound::Sound()
{
    // The offscreen platform has no audio device, and the test suite runs
    // there — creating effects would only produce warnings. Matched on the
    // PREFIX: the plugin takes arguments after a colon (`offscreen:enable_fonts`
    // is the one used here), and an exact comparison reads those spellings as a
    // real display and builds effects against a device that is not there.
    m_available = !qgetenv("QT_QPA_PLATFORM").startsWith("offscreen");
    // Stored like the legibility switch beside it in the toolbar. Without this
    // the mute was discarded on every launch, while README introduces the two
    // as a pair and says the setting is remembered.
    m_muted = QSettings().value(mutedKey(), false).toBool();
}

void Sound::setMuted(bool muted)
{
    if (m_muted == muted)
        return;
    m_muted = muted;
    QSettings().setValue(mutedKey(), muted);
}

void Sound::setVolume(double volume)
{
    m_volume = qBound(0.0, volume, 1.0);
    for (Voices& voices : m_effects)
        for (QSoundEffect* effect : voices.players)
            effect->setVolume(float(m_volume));
}

void Sound::play(const QString& name)
{
    if (m_muted || !m_available)
        return;

    auto it = m_effects.find(name);
    if (it == m_effects.end()) {
        // Built on first use, so a game never pays for sounds it does not play.
        Voices voices;
        voices.players.reserve(kVoices);
        for (int i = 0; i < kVoices; ++i) {
            auto* effect = new QSoundEffect;
            // On the first voice only, or one missing file warns kVoices times.
            // Nothing else reports this: a sound that will not load plays in
            // silence, which is what a muted game sounds like -- so an empty
            // resource, the one failure that takes every effect at once, is
            // indistinguishable from working audio at runtime.
            if (i == 0) {
                QObject::connect(effect, &QSoundEffect::statusChanged, effect, [effect, name] {
                    if (effect->status() == QSoundEffect::Error)
                        qWarning("Sound \"%s\" could not be loaded; it will play silently.",
                                 qPrintable(name));
                });
            }
            effect->setSource(QUrl(QStringLiteral("qrc:/sounds/%1.wav").arg(name)));
            effect->setVolume(float(m_volume));
            voices.players.push_back(effect);
        }
        it = m_effects.insert(name, voices);
    }

    Voices& voices = *it;
    // Round-robin so rapid repeats overlap instead of cutting each other off.
    QSoundEffect* effect = voices.players[std::size_t(voices.next)];
    voices.next = (voices.next + 1) % kVoices;
    effect->play();
}
