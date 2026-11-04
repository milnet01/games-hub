#include "preferencesdialog.h"

#include "donate.h"
#include "legibility.h"
#include "legiblefont.h"
#include "sound.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setObjectName(QStringLiteral("preferencesDialog"));

    // Read once, as the donate dialog does: the switch below changes the
    // games behind this window, and a dialog that re-laid itself out under the
    // pointer would move the very box being clicked.
    const bool large = Legibility::instance().enabled();
    if (large) {
        QFont f = font();
        growByPoints(f, 3.0);
        setFont(f);
    }

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(22, 20, 22, 18);
    outer->setSpacing(12);

    auto* heading = new QLabel(tr("Settings for every game"), this);
    QFont hf = heading->font();
    growByPoints(hf, 4.0);
    hf.setBold(true);
    heading->setFont(hf);
    outer->addWidget(heading);

    m_sound = new QCheckBox(tr("Play game sounds"), this);
    m_sound->setObjectName(QStringLiteral("prefSound"));
    m_sound->setChecked(!Sound::instance().muted());
    outer->addWidget(m_sound);

    auto* volumeRow = new QHBoxLayout;
    auto* volumeLabel = new QLabel(tr("Volume"), this);
    m_volume = new QSlider(Qt::Horizontal, this);
    m_volume->setObjectName(QStringLiteral("prefVolume"));
    m_volume->setRange(0, 100);
    m_volume->setSingleStep(5);
    m_volume->setPageStep(10);
    m_volume->setValue(int(std::lround(Sound::instance().volume() * 100)));
    volumeLabel->setBuddy(m_volume);
    m_volume->setAccessibleName(tr("Volume"));
    m_volumeText = new QLabel(this);
    // Wide enough for "100%" at either size, so the slider does not jump as
    // the number grows a digit.
    m_volumeText->setMinimumWidth(QFontMetrics(m_volumeText->font())
                                      .horizontalAdvance(tr("%1%").arg(100)) + 8);
    volumeRow->addSpacing(28);
    volumeRow->addWidget(volumeLabel);
    volumeRow->addWidget(m_volume, 1);
    volumeRow->addWidget(m_volumeText);
    outer->addLayout(volumeRow);
    showVolume(m_volume->value());

    connect(m_sound, &QCheckBox::toggled, this, [this](bool on) {
        Sound::instance().setMuted(!on);
        m_volume->setEnabled(on);
        if (on)
            Sound::instance().play(Sound::kClick);
    });
    m_volume->setEnabled(m_sound->isChecked());
    connect(m_volume, &QSlider::valueChanged, this, [this](int percent) {
        Sound::instance().setVolume(percent / 100.0);
        showVolume(percent);
        // A click at the new level, so the player hears what they chose. Not
        // while dragging, which would rattle out a click per step.
        if (!m_volume->isSliderDown())
            Sound::instance().play(Sound::kClick);
    });
    connect(m_volume, &QSlider::sliderReleased, this,
            [] { Sound::instance().play(Sound::kClick); });

    auto* legible = new QCheckBox(tr("Large play: bigger cards and higher contrast"), this);
    legible->setObjectName(QStringLiteral("prefLegible"));
    legible->setChecked(large);
    connect(legible, &QCheckBox::toggled, this,
            [](bool on) { Legibility::instance().setEnabled(on); });
    outer->addWidget(legible);

    auto* asking = new QCheckBox(tr("Ask now and then whether you would like to support the project"),
                                 this);
    asking->setObjectName(QStringLiteral("prefDonateAsk"));
    asking->setChecked(donate::asksEnabled());
    connect(asking, &QCheckBox::toggled, this, [](bool on) { donate::setAsksEnabled(on); });
    outer->addWidget(asking);

    auto* note = new QLabel(
        tr("Changes take effect at once and are remembered. Each game's own rules and "
           "difficulty are set inside that game, from its toolbar."),
        this);
    note->setWordWrap(true);
    outer->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    setMinimumWidth(large ? 600 : 480);
}

void PreferencesDialog::showVolume(int percent)
{
    m_volumeText->setText(tr("%1%").arg(percent));
}
