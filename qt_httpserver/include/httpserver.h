#pragma once

#include <QTcpServer>
#include <QHash>
#include <functional>
#include <QThreadPool>
#include <QRegularExpression>
#include <QVector>
#include <QStringList>

class QTcpSocket;

class HttpServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit HttpServer(QObject* parent = nullptr);
    bool start(quint16 port);

    // Register a handler for a path pattern and HTTP method.
    // Pattern supports named parameters like "/user/:id".
    void addHandler(const QString& method, const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler);
    void addGet(const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler);
    void addPost(const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler);
    void addPut(const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler);
    void addDelete(const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler);
    void addAny(const QString& pattern, std::function<QByteArray(const QByteArray& request, const QHash<QString, QString>& params)> handler);

    // Document root for static files. Default is "www". Use empty to disable.
    void setDocumentRoot(const QString& root);

    // Thread-safe entrypoint to request a response write from any thread.
    void postWriteResponse(QTcpSocket* socket, QByteArray response, int status = 200, const QByteArray& contentType = "text/html; charset=utf-8");

private slots:
    void onNewConnection();
    void writeResponse(QTcpSocket* socket, QByteArray response, int status = 200, const QByteArray& contentType = "text/html; charset=utf-8");

private:
    struct Route {
        QString method;
        QString pattern;
        QRegularExpression re;
        QStringList paramNames;
        std::function<QByteArray(const QByteArray&, const QHash<QString, QString>&)> handler;
    };

    QVector<Route> m_routes;
    QThreadPool m_pool;
    QString m_docRoot;
    class Router* m_router = nullptr;
};
