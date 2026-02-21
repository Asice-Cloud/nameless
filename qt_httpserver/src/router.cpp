#include "router.h"
#include <QRegularExpression>

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

void Router::addRoute(const QString& method, const QString& pattern, Handler handler)
{
    Route r;
    r.method = method.toUpper();
    r.pattern = pattern;
    r.re = makeRegexFromPattern(pattern, r.paramNames);
    r.handler = std::move(handler);
    m_routes.append(std::move(r));
}

Router::MatchResult Router::match(const QString& method, const QString& path) const
{
    Router::MatchResult res;
    QString m = method.toUpper();
    for (const auto& r : m_routes) {
        if (r.method != m && r.method != "*") continue;
        QRegularExpressionMatch mres = r.re.match(path);
        if (mres.hasMatch()) {
            QHash<QString, QString> params;
            for (int i = 0; i < r.paramNames.size(); ++i) {
                params.insert(r.paramNames.value(i), mres.captured(i + 1));
            }
            res.handler = r.handler;
            res.params = std::move(params);
            res.matched = true;
            return res;
        }
    }
    return res;
}

QString Router::debugInfo() const
{
    QStringList lines;
    for (const auto& r : m_routes) {
        lines << QString("%1 %2 -> %3").arg(r.method, r.pattern, r.re.pattern());
    }
    return lines.join('\n');
}
