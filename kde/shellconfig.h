#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

/* Per-screen Bendwall assignment. screen -1 in apply() means every screen
   plasmashell reports. The mock file stands in for the D-Bus call. */
struct ScreenAssign {
    int screen = 0;
    QString plugin;
    QString item;
    bool mute = true;
    int volume = 100;
    bool autoMute = false;
};

class ShellConfig : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList screenChoices READ screenChoices NOTIFY screensChanged)
    Q_PROPERTY(bool mute READ mute WRITE setMute NOTIFY muteChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool autoMute READ autoMute WRITE setAutoMute NOTIFY autoMuteChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit ShellConfig(QObject* parent = nullptr);

    QStringList screenChoices() const;
    bool mute() const { return m_mute; }
    int volume() const { return m_volume; }
    bool autoMute() const { return m_auto; }
    QString error() const { return m_error; }
    QString status() const { return m_status; }

    void setMute(bool on);
    void setVolume(int volume);
    void setAutoMute(bool on);
    void setMock(const QString& path);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool apply(const QString& id, int screen);
    Q_INVOKABLE int screenChoiceValue(int index) const;
    Q_INVOKABLE void loadEditor(int screen);
    Q_INVOKABLE QString activeText(const QString& id) const;
    QHash<QString, QString> activeMap() const;

signals:
    void screensChanged();
    void muteChanged();
    void volumeChanged();
    void autoMuteChanged();
    void errorChanged();
    void statusChanged();

private:
    bool readMock();
    bool writeMock();
    bool readDBus();
    bool writeDBus(const QString& id, int screen);
    void setError(const QString& text);
    void setStatus(const QString& text);
    void replaceScreens(const QList<ScreenAssign>& screens);

    QString m_mock;
    QList<ScreenAssign> m_screens;
    bool m_mute = true;
    int m_volume = 100;
    bool m_auto = false;
    QString m_error;
    QString m_status;
};
