#include "BeautySettings.h"
#include <QSettings>
#include <algorithm>
#include <cmath>

const char *BeautySettings::key(int i) {
    static const char *keys[] = {"smoothing", "whitening", "temperature", "tint", "slimming", "eyes", "lipstick", "blush"};
    return keys[i];
}
BeautySettings::BeautySettings(bool persistent, QObject *parent) : QObject(parent), m_persistent(persistent) {
    m_parameters = BeautyParameters::recommended();
    m_saveTimer.setSingleShot(true); m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &BeautySettings::save);
    if (persistent) {
        QSettings s;
        for (int i = 0; i < BeautyParameters::Count; ++i) {
            // Upgrade once to the new lightweight preset. Explicit v2 changes persist.
            float value = s.value("beauty/presetVersion", 0).toInt() >= 2
                ? s.value(QStringLiteral("beauty/") + key(i), m_parameters.values[i]).toFloat()
                : m_parameters.values[i];
            m_parameters.values[i] = std::isfinite(value) ? std::clamp(value, (i == 2 || i == 3) ? -1.f : 0.f, 1.f) : 0;
        }
        m_parameters.enabled = s.value("beauty/presetVersion", 0).toInt() >= 2 ? s.value("beauty/enabled", true).toBool() : true;
        // Migrate the old preset values once; keep intentional user adjustments.
        if (s.value("beauty/presetVersion", 0).toInt() < 3) {
            if (std::abs(m_parameters.values[2] + .04f) < .0001f) m_parameters.values[2] = 0;
            if (std::abs(m_parameters.values[3] - .025f) < .0001f) m_parameters.values[3] = 0;
            if (std::abs(m_parameters.values[1] - .32f) < .0001f) m_parameters.values[1] = .20f;
        }
    }
}
BeautySettings::~BeautySettings() { save(); }
QVariantMap BeautySettings::values() const {
    QVariantMap result;
    for (int i = 0; i < BeautyParameters::Count; ++i) result.insert(QString::fromLatin1(key(i)), m_parameters.values[i]);
    return result;
}
void BeautySettings::setValue(const QString &keyName, double value) {
    if (!std::isfinite(value)) return;
    for (int i = 0; i < BeautyParameters::Count; ++i) if (keyName == QLatin1String(key(i))) {
        float v = std::clamp(float(value), (i == 2 || i == 3) ? -1.f : 0.f, 1.f);
        if (m_parameters.values[i] == v) return;
        m_parameters.values[i] = v; emit changed(); m_saveTimer.start(); return;
    }
}
void BeautySettings::setEnabled(bool value) {
    if (m_parameters.enabled == value) return;
    m_parameters.enabled = value; emit changed(); m_saveTimer.start();
}
void BeautySettings::reset() { m_parameters = BeautyParameters::recommended(); emit changed(); m_saveTimer.start(); }
void BeautySettings::save() {
    if (!m_persistent) return;
    QSettings s;
    s.setValue("beauty/presetVersion", 3);
    for (int i = 0; i < BeautyParameters::Count; ++i) s.setValue(QStringLiteral("beauty/") + key(i), m_parameters.values[i]);
    s.setValue("beauty/enabled", m_parameters.enabled);
}
