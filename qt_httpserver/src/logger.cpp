#include "logger.h"
#include <QDateTime>
#include <QTextStream>

Logger* Logger::instance()
{
    static Logger inst;
    return &inst;
}

Logger::Logger()
    : m_file("server.log")
{
    qint32 errcode = m_file.open(QFile::Append | QFile::Text);
    if (errcode !=0){
        qWarning("Failed to open log file: %d", errcode);
    }
}

Logger::~Logger()
{
    if (m_file.isOpen()) m_file.close();
}

void Logger::log(const QString& text)
{
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen()) {
        if (!m_file.open(QFile::Append | QFile::Text)) return;
    }
    QTextStream ts(&m_file);
    ts << QDateTime::currentDateTime().toString(Qt::ISODate) << " " << text << "\n";
    ts.flush();
}
