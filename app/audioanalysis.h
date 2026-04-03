#ifndef AUDIOANALYSIS_H
#define AUDIOANALYSIS_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QVector>
#ifndef Q_OS_IOS
#include <QProcess>
#endif

class AudioAnalysis : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool analyzing READ analyzing NOTIFY analyzingChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged)
    Q_PROPERTY(int sampleRate READ sampleRate CONSTANT)

public:
    explicit AudioAnalysis(QObject *parent = nullptr);

    bool analyzing() const;
    bool ready() const;
    QString errorString() const;
    QString source() const;
    int sampleRate() const;

    Q_INVOKABLE void analyze(const QUrl &sourceUrl);
    Q_INVOKABLE void reset();
    Q_INVOKABLE qreal levelAt(qreal seconds) const;
    Q_INVOKABLE qreal pulseAt(qreal seconds) const;
    Q_INVOKABLE qreal sweepAt(qreal seconds) const;

signals:
    void analyzingChanged();
    void readyChanged();
    void errorStringChanged();
    void sourceChanged();

private:
    static constexpr int kSampleRate = 120;

    int indexForSeconds(qreal seconds) const;
    void clearAnalysis();
    void finalizeAnalysis();
    void setAnalyzing(bool analyzing);
    void setReady(bool ready);
    void setErrorString(const QString &errorString);
    void stopRunningProcess();

#ifndef Q_OS_IOS
    QProcess m_process;
#endif
    QByteArray m_pcmBytes;
    QVector<qreal> m_levels;
    QVector<qreal> m_pulses;
    QVector<qreal> m_sweeps;
    QString m_errorString;
    QString m_source;
    bool m_analyzing = false;
    bool m_ready = false;
};

#endif // AUDIOANALYSIS_H
