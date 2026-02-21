#include "httpserver.h"
#include "logger.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QDebug>
#include <QRunnable>
#include <QMetaObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QPointer>
#include "router.h"

class RequestRunnable : public QRunnable
{
public:
    using Handler = std::function<QByteArray(const QByteArray&, const QHash<QString, QString>&)>;

    RequestRunnable(Handler handler,
                    QByteArray request,
                    QHash<QString, QString> params,
                    HttpServer* server,
                    QTcpSocket* socket)
        : m_handler(std::move(handler)), m_request(std::move(request)), m_params(std::move(params)), m_server(server), m_socket(socket)
    {}

    void run() override
    {
        QByteArray body = m_handler ? m_handler(m_request, m_params) : QByteArray();

        QPointer<QTcpSocket> sock = m_socket;
        HttpServer* server = m_server;
        QMetaObject::invokeMethod(server, [server, sock, body]() {
            if (sock)
                server->postWriteResponse(sock, body);
        }, Qt::QueuedConnection);
    }

private:
    Handler m_handler;
    QByteArray m_request;
    QHash<QString, QString> m_params;
    HttpServer* m_server;
    QPointer<QTcpSocket> m_socket;
};

static QRegularExpression makeRegexFromPattern(const QString& pattern, QStringList& outParamNames)
{
    outParamNames.clear();
    QRegularExpression tokenRx(R"((:\w+)|\*)");
    int offset = 0;
    QRegularExpressionMatch match = tokenRx.match(pattern, offset);
    QString result;
    int lastPos = 0;
    while (match.hasMatch()) {
        int pos = match.capturedStart();
        result += QRegularExpression::escape(pattern.mid(lastPos, pos - lastPos));
        QString token = match.captured(0);
        if (token == "*") {
            result += "(.*)";
            outParamNames << QStringLiteral("splat");
        } else if (token.startsWith(':')) {
            result += "([^/]+)";
            outParamNames << token.mid(1);
        }
        lastPos = match.capturedEnd();
        offset = lastPos;
        match = tokenRx.match(pattern, offset);
    }
    result += QRegularExpression::escape(pattern.mid(lastPos));
    return QRegularExpression(QStringLiteral("^") + result + QStringLiteral("$"));
}

HttpServer::HttpServer(QObject* parent)
    : QTcpServer(parent)
{
    m_pool.setMaxThreadCount(QThread::idealThreadCount());
    connect(this, &QTcpServer::newConnection, this, &HttpServer::onNewConnection);
    m_docRoot = QStringLiteral("www");
    m_router = new Router();
}

bool HttpServer::start(quint16 port)
{
    if (!listen(QHostAddress::Any, port)) {
        qWarning() << "HttpServer: listen failed:" << errorString();
        return false;
    }
    qDebug() << "HttpServer: listening on port" << serverPort();
    return true;
}

void HttpServer::addHandler(const QString& method, const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler)
{
    if (m_router) m_router->addRoute(method, pattern, std::move(handler));
    // Log registration for diagnostics
    if (m_router) {
        QString info = QString("Registered route: %1 %2").arg(method, pattern);
        Logger::instance()->log(info);
        Logger::instance()->log(m_router->debugInfo());
    }
}

void HttpServer::addGet(const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler)
{
    addHandler(QStringLiteral("GET"), pattern, std::move(handler));
}

void HttpServer::addPost(const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler)
{
    addHandler(QStringLiteral("POST"), pattern, std::move(handler));
}

void HttpServer::addPut(const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler)
{
    addHandler(QStringLiteral("PUT"), pattern, std::move(handler));
}

void HttpServer::addDelete(const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler)
{
    addHandler(QStringLiteral("DELETE"), pattern, std::move(handler));
}

void HttpServer::addAny(const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler)
{
    addHandler(QStringLiteral("*"), pattern, std::move(handler));
}

void HttpServer::postWriteResponse(QTcpSocket* socket, QByteArray response, int status, const QByteArray& contentType)
{
    // This runs in the server (main) thread when invoked via QMetaObject::invokeMethod with Qt::QueuedConnection
    writeResponse(socket, response, status, contentType);
}

void HttpServer::setDocumentRoot(const QString& root)
{
    m_docRoot = root;
}

void HttpServer::onNewConnection()
{
    while (hasPendingConnections()) {
        QTcpSocket* socket = nextPendingConnection();

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            QByteArray request = socket->readAll();
            Logger::instance()->log(QString("Received request from %1: %2").arg(socket->peerAddress().toString()).arg(QString::fromUtf8(request)));

            // Very small HTTP request parsing (only method and path)
            QList<QByteArray> lines = request.split('\n');
            QByteArray first = lines.isEmpty() ? QByteArray() : lines.first().trimmed();
            QByteArray method;
            QByteArray path;
            if (!first.isEmpty()) {
                QList<QByteArray> parts = first.split(' ');
                if (parts.size() >= 2) {
                    method = parts.at(0);
                    path = parts.at(1);
                }
            }

            QString pathStr = QString::fromUtf8(path);
            QString methodStr = QString::fromUtf8(method).trimmed();

            // Normalize path: strip query string and fragment, and remove trailing slash (except root)
            int qpos = pathStr.indexOf('?');
            if (qpos != -1) pathStr = pathStr.left(qpos);
            int hpos = pathStr.indexOf('#');
            if (hpos != -1) pathStr = pathStr.left(hpos);
            if (pathStr.endsWith('/') && pathStr.size() > 1) pathStr.chop(1);
            pathStr = pathStr.trimmed();

            bool handled = false;
            if (m_router) {
                Logger::instance()->log(QString("Routing attempt: method=%1 path=%2").arg(methodStr, pathStr));
                auto mr = m_router->match(methodStr, pathStr);
                if (mr.matched) {
                    Logger::instance()->log(QString("Route matched for %1 %2").arg(methodStr, pathStr));
                    RequestRunnable* task = new RequestRunnable(mr.handler, request, mr.params, this, socket);
                    task->setAutoDelete(true);
                    m_pool.start(task);
                    handled = true;
                }
            }

            if (!handled) {
                qDebug() << "Router: no match for" << methodStr << pathStr;
                if (m_router) qDebug().noquote() << m_router->debugInfo();

                if (method == "GET" && !m_docRoot.isEmpty()) {
                    // Try to serve static file from document root
                    QString rel = pathStr;
                    if (rel.startsWith('/')) rel.remove(0, 1);
                    if (rel.isEmpty()) rel = "index.html";
                    QDir doc(m_docRoot);
                    QString absPath = doc.absoluteFilePath(rel);
                    QFileInfo fi(absPath);
                    // Prevent directory traversal: ensure file is inside doc root
                    if (fi.exists() && fi.isFile() && fi.absoluteFilePath().startsWith(doc.absolutePath())) {
                        QFile f(fi.absoluteFilePath());
                        if (f.open(QFile::ReadOnly)) {
                            QByteArray data = f.readAll();
                            QMimeDatabase db;
                            QMimeType mt = db.mimeTypeForFile(fi.absoluteFilePath());
                            QByteArray mime = mt.isValid() ? mt.name().toUtf8() : QByteArray("application/octet-stream");
                            // send via queued lambda to avoid metatype issues; use QPointer to avoid dangling socket
                            {
                                QPointer<QTcpSocket> sock(socket);
                                HttpServer* srv = this;
                                QMetaObject::invokeMethod(srv, [srv, sock, data, mime]() {
                                    if (sock) srv->postWriteResponse(sock, data, 200, mime);
                                }, Qt::QueuedConnection);
                            }
                            return;
                        }
                    }
                }

                // default responses handled here
                QByteArray body;
                int status = 200;
                if (method != "GET") {
                    status = 405;
                    body = "<html><body><h1>Method Not Allowed</h1></body></html>";
                } else {
                    status = 404;
                    body = "<html><body><h1>Not Found</h1></body></html>";
                }
                // write directly
                writeResponse(socket, body, status);
            }
        });

        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void HttpServer::writeResponse(QTcpSocket* socket, QByteArray response, int status, const QByteArray& contentType)
{
    if (!socket) return;

    QByteArray statusLine;
    if (status == 200) statusLine = "HTTP/1.1 200 OK\r\n";
    else if (status == 404) statusLine = "HTTP/1.1 404 Not Found\r\n";
    else if (status == 405) statusLine = "HTTP/1.1 405 Method Not Allowed\r\n";
    else statusLine = "HTTP/1.1 200 OK\r\n";

    QByteArray full;
    full += statusLine;
    full += "Content-Type: " + contentType + "\r\n";
    full += "Content-Length: " + QByteArray::number(response.size()) + "\r\n";
    full += "Connection: close\r\n";
    full += "\r\n";
    full += response;

    socket->write(full);
    socket->disconnectFromHost();

    Logger::instance()->log(QString("Responded to %1 with status %2, %3 bytes").arg(socket->peerAddress().toString()).arg(status).arg(response.size()));
}
