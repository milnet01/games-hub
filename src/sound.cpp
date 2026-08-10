#include "sound.h"

#include <QSoundEffect>
#include <QUrl>

Sound& Sound::instance()
{
    static Sound sound;
    return sound;
}

Sound::Sound()
{
    // The offscreen platform has no audio device, and the test suite runs
    // there — creating effects would only produce warnings.
    m_available = qgetenv("QT_QPA_PLATFORM") != "offscreen";
}

void Sound::setMuted(bool muted)
{
    m_muted = muted;
}

void Sound::setVolume(double volume)
{
    m_volume = qBound(0.0, volume, 1.0);
    for (Voices& voices : m_effects)
        for (QSoundEffect* effect : voices.players)
            effect->setVolume(m_volume);
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
            effect->setSource(QUrl(QStringLiteral("qrc:/sounds/%1.wav").arg(name)));
            effect->setVolume(m_volume);
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
