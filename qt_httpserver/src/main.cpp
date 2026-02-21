#include <QCoreApplication>
#include <QDebug>
#include "httpserver.h"

int main(int argc, char* argv[])
{
    QCoreApplication a(argc, argv);

    HttpServer server;
    server.setDocumentRoot("www");
    // Example routes
    server.addGet("/hello", [](const QByteArray&, const QHash<QString, QString>&){
        return QByteArray("<html><body><h1>Hello from /hello</h1></body></html>");
    });

    server.addGet("/user/:id", [](const QByteArray&, const QHash<QString, QString>& params){
        QString id = params.value("id");
        QByteArray body = "<html><body><h1>User: ";
        body += id.toUtf8();
        body += "</h1></body></html>";
        return body;
    });

    // POST example: echo body
    server.addPost("/echo", [](const QByteArray& req, const QHash<QString, QString>&){
        // naive: find blank line separator and return body
        QList<QByteArray> parts = req.split('\n');
        int i = 0;
        for (; i < parts.size(); ++i) {
            if (parts[i].trimmed().isEmpty()) break;
        }
        QByteArray body;
        if (i + 1 < parts.size()) {
            // join remaining lines as body
            for (int j = i + 1; j < parts.size(); ++j) body += parts[j] + '\n';
        }
        QByteArray out = "<html><body><h1>Echo</h1><pre>" + QString::fromUtf8(body).toHtmlEscaped().toUtf8() + "</pre></body></html>";
        return out;
    });
    if (!server.start(8080)) {
        qWarning() << "Failed to start HTTP server";
        return 1;
    }

    qDebug() << "HTTP server running on port 8080";
    return QCoreApplication::exec();
}
