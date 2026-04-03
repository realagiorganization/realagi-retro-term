#ifndef MIDIRENDERER_H
#define MIDIRENDERER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>
#ifndef Q_OS_IOS
#include <QProcess>
#endif

class MidiRenderer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool rendering READ rendering NOTIFY renderingChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged)
    Q_PROPERTY(QUrl outputUrl READ outputUrl NOTIFY outputUrlChanged)
    Q_PROPERTY(QString soundFontPath READ soundFontPath NOTIFY soundFontPathChanged)

public:
    explicit MidiRenderer(QObject *parent = nullptr);

    bool rendering() const;
    bool ready() const;
    QString errorString() const;
    QString source() const;
    QUrl outputUrl() const;
    QString soundFontPath() const;

    Q_INVOKABLE bool isMidiFile(const QUrl &sourceUrl) const;
    Q_INVOKABLE void render(const QUrl &sourceUrl);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void reset();

signals:
    void renderingChanged();
    void readyChanged();
    void errorStringChanged();
    void sourceChanged();
    void outputUrlChanged();
    void soundFontPathChanged();

private:
    QString detectSoundFont() const;
    QString ensureOutputPath(const QString &sourcePath) const;
    QString resolveSourcePath(const QUrl &sourceUrl) const;
    void setErrorString(const QString &errorString);
    void setOutputUrl(const QUrl &outputUrl);
    void setReady(bool ready);
    void setRendering(bool rendering);
    void setSource(const QString &source);
    void setSoundFontPath(const QString &soundFontPath);
    void stopRunningProcess();

#ifndef Q_OS_IOS
    QProcess m_process;
#endif
    QByteArray m_errorBytes;
    QString m_errorString;
    QString m_pendingOutputPath;
    QUrl m_outputUrl;
    QString m_soundFontPath;
    QString m_source;
    bool m_rendering = false;
    bool m_ready = false;
};

#endif // MIDIRENDERER_H
