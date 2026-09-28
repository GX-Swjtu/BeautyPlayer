#pragma once
#include "BeautyParameters.h"
#include <QObject>
#include <QVariantMap>
#include <QTimer>

class BeautySettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
public:
    explicit BeautySettings(bool persistent, QObject *parent = nullptr);
    ~BeautySettings() override;
    QVariantMap values() const;
    bool enabled() const { return m_parameters.enabled; }
    void setEnabled(bool value);
    BeautyParameters snapshot() const { return m_parameters; }
    Q_INVOKABLE void setValue(const QString &key, double value);
    Q_INVOKABLE void reset();
    static const char *key(int index);
signals:
    void changed();
private:
    void save();
    BeautyParameters m_parameters;
    bool m_persistent;
    QTimer m_saveTimer;
};
