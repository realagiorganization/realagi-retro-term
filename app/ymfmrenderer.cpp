#include "ymfmrenderer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include "../third_party/ymfm/examples/vgmrender/vgmrender.h"

namespace {
bool cancellationRequested(const std::shared_ptr<std::atomic_bool> &cancelRequested)
{
    return cancelRequested && cancelRequested->load();
}

QString renderErrorForCode(int errorCode)
{
    switch (errorCode) {
    case 1:
        return QObject::tr("Internal YMFM render arguments were invalid.");
    case 2:
        return QObject::tr("Could not open the selected VGM/VGZ file.");
    case 3:
        return QObject::tr("Could not read the selected VGM/VGZ file.");
    case 4:
        return QObject::tr("The selected file is not a valid VGM/VGZ track.");
    case 5:
        return QObject::tr("No supported Yamaha FM chips were found in the selected track.");
    case 6:
    case 7:
        return QObject::tr("YMFM could not write the rendered FM audio cache file.");
    default:
        return QObject::tr("YMFM could not render the selected FM track.");
    }
}
} // namespace

YmfmRenderer::YmfmRenderer(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<YmfmRenderResult>::finished, this, [this]() {
        const YmfmRenderResult result = m_watcher.result();
        setRendering(false);
        m_cancelRequested.reset();

        if (result.serial != m_renderSerial || result.cancelled) {
            return;
        }

        if (!result.errorString.isEmpty()) {
            QFile::remove(m_pendingOutputPath);
            setOutputUrl(QUrl());
            setReady(false);
            setErrorString(result.errorString);
            return;
        }

        setFormatName(result.formatName);
        setSystemName(result.systemName);
        setOutputUrl(QUrl::fromLocalFile(result.outputPath));
        setErrorString(QString());
        setReady(true);
        m_pendingOutputPath.clear();
    });
}

YmfmRenderer::~YmfmRenderer()
{
    stopRunningRender();
}

bool YmfmRenderer::rendering() const
{
    return m_rendering;
}

bool YmfmRenderer::ready() const
{
    return m_ready;
}

QString YmfmRenderer::errorString() const
{
    return m_errorString;
}

QString YmfmRenderer::source() const
{
    return m_source;
}

QUrl YmfmRenderer::outputUrl() const
{
    return m_outputUrl;
}

QString YmfmRenderer::formatName() const
{
    return m_formatName;
}

QString YmfmRenderer::systemName() const
{
    return m_systemName;
}

bool YmfmRenderer::isVgmFile(const QUrl &sourceUrl) const
{
    return !formatNameForPath(resolveSourcePath(sourceUrl)).isEmpty();
}

void YmfmRenderer::render(const QUrl &sourceUrl)
{
    const QString sourcePath = resolveSourcePath(sourceUrl);
    if (sourcePath.isEmpty()) {
        reset();
        setErrorString(tr("Select a local VGM/VGZ track first."));
        return;
    }

    const QString detectedFormat = formatNameForPath(sourcePath);
    if (detectedFormat.isEmpty()) {
        reset();
        setErrorString(tr("The selected file is not a supported VGM/VGZ track."));
        return;
    }

    stopRunningRender();

    setRendering(false);
    setReady(false);
    setErrorString(QString());
    setOutputUrl(QUrl());
    setSource(sourcePath);
    setFormatName(detectedFormat);
    setSystemName(systemNameForPath(sourcePath));

    m_pendingOutputPath = ensureOutputPath(sourcePath, detectedFormat);
    const QFileInfo outputInfo(m_pendingOutputPath);
    if (outputInfo.exists() && outputInfo.size() > 44) {
        setOutputUrl(QUrl::fromLocalFile(outputInfo.absoluteFilePath()));
        setReady(true);
        m_pendingOutputPath.clear();
        return;
    }

    QFile::remove(m_pendingOutputPath);

    const quint64 serial = ++m_renderSerial;
    const QString outputPath = m_pendingOutputPath;
    const QString systemName = m_systemName;
    m_cancelRequested = std::make_shared<std::atomic_bool>(false);
    setRendering(true);
    m_watcher.setFuture(QtConcurrent::run([serial, sourcePath, outputPath, detectedFormat, systemName, cancelRequested = m_cancelRequested]() {
        return renderToWave(serial, sourcePath, outputPath, detectedFormat, systemName, cancelRequested);
    }));
}

void YmfmRenderer::cancel()
{
    const bool hadRunningRender = m_watcher.isRunning();
    stopRunningRender();
    if (hadRunningRender && !m_pendingOutputPath.isEmpty()) {
        QFile::remove(m_pendingOutputPath);
    }
    setRendering(false);
    setReady(false);
    setErrorString(QString());
    setOutputUrl(QUrl());
}

void YmfmRenderer::reset()
{
    cancel();
    m_pendingOutputPath.clear();
    setSource(QString());
    setFormatName(QString());
    setSystemName(QString());
}

YmfmRenderResult YmfmRenderer::renderToWave(quint64 serial,
                                            const QString &sourcePath,
                                            const QString &outputPath,
                                            const QString &formatName,
                                            const QString &systemName,
                                            std::shared_ptr<std::atomic_bool> cancelRequested)
{
    YmfmRenderResult result;
    result.serial = serial;
    result.sourcePath = sourcePath;
    result.outputPath = outputPath;
    result.formatName = formatName;
    result.systemName = systemName;

    if (cancellationRequested(cancelRequested)) {
        result.cancelled = true;
        return result;
    }

    const QByteArray sourceBytes = sourcePath.toUtf8();
    const QByteArray outputBytes = outputPath.toUtf8();
    const int renderCode = ymfm_vgmrender_file(sourceBytes.constData(),
                                               outputBytes.constData(),
                                               kSampleRate,
                                               cancelRequested ? cancelRequested.get() : nullptr);
    if (renderCode == 8 || cancellationRequested(cancelRequested)) {
        result.cancelled = true;
        return result;
    }

    if (renderCode != 0) {
        result.errorString = renderErrorForCode(renderCode);
        return result;
    }

    const QFileInfo outputInfo(outputPath);
    if (!outputInfo.exists() || outputInfo.size() <= 44) {
        result.errorString = QObject::tr("YMFM finished without producing rendered audio.");
    }

    return result;
}

QString YmfmRenderer::ensureOutputPath(const QString &sourcePath, const QString &formatName) const
{
    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        cacheRoot = QDir::tempPath() + QStringLiteral("/realagi-retro-term");
    }

    QDir cacheDir(cacheRoot);
    cacheDir.mkpath(QStringLiteral("ymfm-renders"));
    cacheDir.cd(QStringLiteral("ymfm-renders"));

    const QFileInfo sourceInfo(sourcePath);
    const QByteArray cacheKey = sourceInfo.absoluteFilePath().toUtf8()
            + '|'
            + QByteArray::number(sourceInfo.size())
            + '|'
            + QByteArray::number(sourceInfo.lastModified().toMSecsSinceEpoch())
            + '|'
            + formatName.toUtf8();
    const QString hashedName = QString::fromLatin1(
                QCryptographicHash::hash(cacheKey, QCryptographicHash::Sha1).toHex().left(16));
    return cacheDir.absoluteFilePath(hashedName + QStringLiteral(".wav"));
}

QString YmfmRenderer::formatNameForPath(const QString &sourcePath) const
{
    if (sourcePath.isEmpty()) {
        return QString();
    }

    const QFileInfo sourceInfo(sourcePath);
    const QString extension = sourceInfo.suffix().toLower();
    if (extension == QStringLiteral("vgm") || extension == QStringLiteral("vgz")) {
        return extension.toUpper();
    }

    return QString();
}

QString YmfmRenderer::resolveSourcePath(const QUrl &sourceUrl) const
{
    if (!sourceUrl.isValid()) {
        return QString();
    }

    return sourceUrl.isLocalFile() ? sourceUrl.toLocalFile() : sourceUrl.toString();
}

QString YmfmRenderer::systemNameForPath(const QString &sourcePath) const
{
    if (formatNameForPath(sourcePath).isEmpty()) {
        return QString();
    }

    return QStringLiteral("Sega/Yamaha FM");
}

void YmfmRenderer::setErrorString(const QString &errorString)
{
    if (m_errorString == errorString) {
        return;
    }

    m_errorString = errorString;
    emit errorStringChanged();
}

void YmfmRenderer::setFormatName(const QString &formatName)
{
    if (m_formatName == formatName) {
        return;
    }

    m_formatName = formatName;
    emit formatNameChanged();
}

void YmfmRenderer::setOutputUrl(const QUrl &outputUrl)
{
    if (m_outputUrl == outputUrl) {
        return;
    }

    m_outputUrl = outputUrl;
    emit outputUrlChanged();
}

void YmfmRenderer::setReady(bool ready)
{
    if (m_ready == ready) {
        return;
    }

    m_ready = ready;
    emit readyChanged();
}

void YmfmRenderer::setRendering(bool rendering)
{
    if (m_rendering == rendering) {
        return;
    }

    m_rendering = rendering;
    emit renderingChanged();
}

void YmfmRenderer::setSource(const QString &source)
{
    if (m_source == source) {
        return;
    }

    m_source = source;
    emit sourceChanged();
}

void YmfmRenderer::setSystemName(const QString &systemName)
{
    if (m_systemName == systemName) {
        return;
    }

    m_systemName = systemName;
    emit systemNameChanged();
}

void YmfmRenderer::stopRunningRender()
{
    if (!m_watcher.isRunning()) {
        return;
    }

    if (m_cancelRequested) {
        m_cancelRequested->store(true);
    }

    m_watcher.waitForFinished();
    m_cancelRequested.reset();
}
