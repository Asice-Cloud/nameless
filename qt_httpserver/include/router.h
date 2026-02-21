#pragma once

#include <QString>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>
#include <QHash>
#include <functional>

class Router
{
public:
    using Handler = std::function<QByteArray(const QByteArray&, const QHash<QString, QString>&)>;

    struct MatchResult {
        Handler handler;
        QHash<QString, QString> params;
        bool matched = false;
    };

    void addRoute(const QString& method, const QString& pattern, Handler handler);
    MatchResult match(const QString& method, const QString& path) const;
    QString debugInfo() const;

private:
    struct Route {
        QString method;
        QString pattern;
        QRegularExpression re;
        QStringList paramNames;
        Handler handler;
    };

    QVector<Route> m_routes;
};
