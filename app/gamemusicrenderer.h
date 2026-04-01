#ifndef GAMEMUSICRENDERER_H
#define GAMEMUSICRENDERER_H

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>

#include <atomic>
#include <memory>

struct GameMusicRenderResult
{
    quint64 serial = 0;
    bool cancelled = false;
    QString sourcePath;
    QString outputPath;
    QString errorString;
    QString formatName;
    QString systemName;
};

class GameMusicRenderer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool rendering READ rendering NOTIFY renderingChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged)
    Q_PROPERTY(QUrl outputUrl READ outputUrl NOTIFY outputUrlChanged)
    Q_PROPERTY(QString formatName READ formatName NOTIFY formatNameChanged)
    Q_PROPERTY(QString systemName READ systemName NOTIFY systemNameChanged)

public:
    explicit GameMusicRenderer(QObject *parent = nullptr);
    ~GameMusicRenderer() override;

    bool rendering() const;
    bool ready() const;
    QString errorString() const;
    QString source() const;
    QUrl outputUrl() const;
    QString formatName() const;
    QString systemName() const;

    Q_INVOKABLE bool isGameMusicFile(const QUrl &sourceUrl) const;
    Q_INVOKABLE void render(const QUrl &sourceUrl);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void reset();

signals:
    void renderingChanged();
    void readyChanged();
    void errorStringChanged();
    void sourceChanged();
    void outputUrlChanged();
    void formatNameChanged();
    void systemNameChanged();

private:
    static constexpr int kSampleRate = 44100;

    static GameMusicRenderResult renderToWave(quint64 serial,
                                              const QString &sourcePath,
                                              const QString &outputPath,
                                              const QString &formatName,
                                              const QString &systemName,
                                              std::shared_ptr<std::atomic_bool> cancelRequested);

    QString ensureOutputPath(const QString &sourcePath, const QString &formatName) const;
    QString formatNameForPath(const QString &sourcePath) const;
    QString resolveSourcePath(const QUrl &sourceUrl) const;
    QString systemNameForPath(const QString &sourcePath) const;
    void setErrorString(const QString &errorString);
    void setFormatName(const QString &formatName);
    void setOutputUrl(const QUrl &outputUrl);
    void setReady(bool ready);
    void setRendering(bool rendering);
    void setSource(const QString &source);
    void setSystemName(const QString &systemName);
    void stopRunningRender();

    QFutureWatcher<GameMusicRenderResult> m_watcher;
    std::shared_ptr<std::atomic_bool> m_cancelRequested;
    QString m_errorString;
    QString m_formatName;
    QString m_pendingOutputPath;
    QUrl m_outputUrl;
    QString m_source;
    QString m_systemName;
    quint64 m_renderSerial = 0;
    bool m_rendering = false;
    bool m_ready = false;
};

#endif // GAMEMUSICRENDERER_H
