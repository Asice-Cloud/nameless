#pragma once

#include <QString>
#include <QFile>
#include <QMutex>

class Logger
{
public:
    static Logger* instance();
    void log(const QString& text);
private:
    Logger();
    ~Logger();
    QFile m_file;
    QMutex m_mutex;
};
