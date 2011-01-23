#include "qimagegrabbermjpeg.h"

QImageGrabberMjpeg::QImageGrabberMjpeg(QObject *parent)
    : QImageGrabber(parent)
{
    downloadManager = new QNetworkAccessManager(this);
    connect(downloadManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(downloadFinished(QNetworkReply*)));
    request = new QNetworkRequest();
    request->setRawHeader("User-Agent", "Mars2020 Imagegrabber LIB");
    reply = NULL;

    QImageGrabberParameter boundaryParam;
    boundaryParam.name = tr("Boundary");
    boundaryParam.value = "boundarydonotcross";
}

bool QImageGrabberMjpeg::isGrabbing()
{
    return currentState == GrabbingOn;
}

void QImageGrabberMjpeg::setUrl(QUrl &url)
{
    if (this->isGrabbing()) {
        this->stopGrabbing();
        currentUrl = url;
        this->sendRequest();
    } else {
        currentUrl = url;
    }
}

void QImageGrabberMjpeg::startGrabbing()
{
    sendRequest();
}

void QImageGrabberMjpeg::stopGrabbing()
{
    currentState = GrabbingOff;
    emit stateChanged(GrabbingOff);
    if (reply != NULL) {
        reply->abort();
    }
}


void QImageGrabberMjpeg::downloadFinished(QNetworkReply *reply)
{
    imageReader->setDevice(reply);
    qWarning() << "start reading image";
    imageReader->read(currentImage);
    qWarning() << "reading image done";
    emit newImageGrabbed(currentImage);
}

void QImageGrabberMjpeg::downloadErrorSlot(QNetworkReply::NetworkError )
{
    if (reply != NULL) {
        errorStr = reply->errorString();
        emit errorHappend();
    }
}

void QImageGrabberMjpeg::sendRequest()
{
    qWarning() << currentUrl;
    request->setUrl(currentUrl);
    reply = downloadManager->get(*request);
    connect(reply , SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(downloadErrorSlot(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(readyRead()), this, SLOT(replyDataAvailable()));
    mjpgState = MjpgBoundary;
    streamType = StreamTypeUnknown;
    currentImageSize = 0;
    imageBuffer->seek(0);
    requestTime = QTime::currentTime();
}

void QImageGrabberMjpeg::replyDataAvailable()
{
    if (currentState != GrabbingOn) {
        currentState = GrabbingOn;
        emit stateChanged(currentState);
    }
    QString cLine;
    if (mjpgState == MjpgBoundary) {
        if (streamType == StreamTypeUnknown) {
            if (reply->bytesAvailable() >= 50) {
                cLine = reply->readLine(51);
                qWarning() << cLine;
                if (cLine.startsWith("mjpeg")) {
                    streamType = StreamTypeWebcamXP;
                    bool ok = false;
                    currentImageSize = cLine.mid(5,8).toInt(&ok);
                    if (ok) {
                        imageBuffer->seek(0);
                        mjpgState = MjpgJpg;
                        qWarning() << currentImageSize << "CI" << cLine;
                    } else {
                        qWarning() << QString("Could not convert %1 to number").arg(cLine.mid(5,7));
                    }
                    // we need to seek a bit
                } else {
                    streamType = StreamTypeMjpgStreamer;
                }
            } else { // to few bytes came so return the next readyíread will take us further
                return;
            }
        }

        if (mjpgState == MjpgBoundary) {
            if (streamType == StreamTypeWebcamXP) {
                if (reply->bytesAvailable() >= 50) {
                    QByteArray borderArray = reply->read(51);
                    qWarning() << borderArray;
                    if (!borderArray.startsWith("mjpeg")) {
                        qWarning() << "invalid border" << borderArray;
                        return;
                    } else {
                        bool ok = false;
                        currentImageSize = borderArray.mid(5,8).toInt(&ok);
                        if (ok) {
                            imageBuffer->seek(0);
                            mjpgState = MjpgJpg;
                            qWarning() << currentImageSize << "CI" << borderArray;
                        } else {
                            qWarning() << QString("Could not convert %1 to number").arg(QString(borderArray.mid(5,7)));
                        }
                    }
                } else {  // to few bytes came so return the next readyíread will take us further
                    return;
                }
            } else if(streamType == StreamTypeMjpgStreamer) {
                bool quitNext = false;
                while(reply->canReadLine()) {
                    QString cLine = reply->readLine();
                    if (quitNext)
                        break;
                    if (cLine.startsWith("Content-Length:")) {
                        bool ok = false;
                        currentImageSize = cLine.mid(16).toInt(&ok);
                        qWarning() << currentImageSize;
                        if (!ok) {
                            qWarning() << QString("Could not convert %1 to number").arg(cLine.mid(16));
                            return;
                        }
                    } else if (cLine.startsWith("X-Timestamp:")) {
                        // FIXME read timestamp;
                        mjpgState = MjpgJpg;
                        quitNext = true;
                    }
                }
            }
        }
    }

    if (mjpgState == MjpgJpg) {
        imageBuffer->write(reply->read(currentImageSize - imageBuffer->pos()));
        if (imageBuffer->pos() == currentImageSize) {
            imageReader->setDevice(imageBuffer);
            imageBuffer->seek(0);
            imageReader->read(currentImage);
            imageBuffer->seek(0);

            emit newImageGrabbed(currentImage);
            calcFPS(requestTime.msecsTo(QTime::currentTime()));
            requestTime = QTime::currentTime();
            mjpgState = MjpgBoundary;
        }
    }

    /*if (reply->bytesAvailable()) {
        qWarning() << "LEFT" << reply->bytesAvailable() << "state" << mjpgState;
        replyDataAvailable();
    }*/
}

void QImageGrabberMjpeg::setSource(QString str)
{
    QUrl checkUrl(str);
    if (checkUrl.isValid()) {
        currentUrl = checkUrl;
    } else {
        qWarning() << "invalid URL:" << str;
    }
}

void QImageGrabberMjpeg::newImageTimeOut()
{
    qWarning() << "Hogy a faszba hívódott ez itt meg";
}
