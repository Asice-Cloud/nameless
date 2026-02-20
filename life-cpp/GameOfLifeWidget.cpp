#include "GameOfLifeWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <random>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <QVector>
#include <QDateTime>
#include <algorithm>
#include <QScrollArea>
#include <QScrollBar>

GameOfLifeWidget::GameOfLifeWidget(QWidget *parent)
    : QWidget(parent), cols(80), rows(60), cellSize(10), running(false)
{
    resizeGrid();
    // Prevent the layout from compressing the drawing area below the grid size
    setMinimumSize(cols * cellSize, rows * cellSize);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    timer.setInterval(100);
    connect(&timer, &QTimer::timeout, this, &GameOfLifeWidget::step);
    setFocusPolicy(Qt::StrongFocus);
    loggingEnabled = false;
    generation = 0;
}

QSize GameOfLifeWidget::sizeHint() const {
    return QSize(cols * cellSize, rows * cellSize);
}

void GameOfLifeWidget::resizeGrid() {
    grid.assign(rows, std::vector<uint8_t>(cols, 0));
    nextGrid = grid;
    update();
}

int GameOfLifeWidget::countNeighbors(int r, int c) const {
    int cnt = 0;
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            int rr = r + dr;
            int cc = c + dc;
            if (rr >= 0 && rr < rows && cc >= 0 && cc < cols) {
                cnt += grid[rr][cc];
            }
        }
    }
    return cnt;
}

void GameOfLifeWidget::step() {
    // expand the grid if any live cell touches the current border
    expandIfNeeded();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int n = countNeighbors(r, c);
            if (grid[r][c]) {
                nextGrid[r][c] = (n == 2 || n == 3) ? 1 : 0;
            } else {
                nextGrid[r][c] = (n == 3) ? 1 : 0;
            }
        }
    }
    grid.swap(nextGrid);
    generation++;
    if (loggingEnabled) {
        int alive = 0;
        for (int r = 0; r < rows; ++r) for (int c = 0; c < cols; ++c) alive += grid[r][c];
        logMessage(QString("Generation %1: alive=%2").arg(generation).arg(alive));
        dumpGridSnapshot(QString("Generation %1 snapshot").arg(generation));
    }
    update();
}

void GameOfLifeWidget::clear() {
    for (auto &row : grid) std::fill(row.begin(), row.end(), 0);
    update();
}

void GameOfLifeWidget::randomize() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> d(0, 1);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            grid[r][c] = d(gen);
    update();
}

void GameOfLifeWidget::toggleRunning() {
    setRunning(!running);
}

void GameOfLifeWidget::setRunning(bool run) {
    running = run;
    if (running) timer.start(); else timer.stop();
}

void GameOfLifeWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    p.setPen(Qt::lightGray);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            QRect cellRect(c * cellSize, r * cellSize, cellSize, cellSize);
            if (grid[r][c]) {
                p.fillRect(cellRect.adjusted(1,1,-1,-1), Qt::black);
            } else {
                p.drawRect(cellRect);
            }
        }
    }
}

void GameOfLifeWidget::mousePressEvent(QMouseEvent *event) {
    int c = event->pos().x() / cellSize;
    int r = event->pos().y() / cellSize;
    if (r >= 0 && r < rows && c >= 0 && c < cols) {
        if (event->button() == Qt::LeftButton) {
            drawValue = 1;
            drawing = true;
            grid[r][c] = 1;
        } else if (event->button() == Qt::RightButton) {
            drawValue = 0;
            drawing = true;
            grid[r][c] = 0;
        } else if (event->button() == Qt::MiddleButton) {
            // start panning
            panning = true;
            panStartPos = event->globalPosition().toPoint();
            // find enclosing QScrollArea
            QWidget *w = this;
            panStartH = 0; panStartV = 0;
            while (w) {
                if (auto sa = qobject_cast<QScrollArea*>(w->parentWidget())) {
                    panStartH = sa->horizontalScrollBar()->value();
                    panStartV = sa->verticalScrollBar()->value();
                    break;
                }
                w = w->parentWidget();
            }
        } else {
            // other buttons toggle
            grid[r][c] = grid[r][c] ? 0 : 1;
        }
        update();
    }
}

void GameOfLifeWidget::mouseMoveEvent(QMouseEvent *event) {
    if (panning) {
        QPoint delta = event->globalPosition().toPoint() - panStartPos;
        QWidget *w = this;
        while (w) {
            if (auto sa = qobject_cast<QScrollArea*>(w->parentWidget())) {
                sa->horizontalScrollBar()->setValue(panStartH - delta.x());
                sa->verticalScrollBar()->setValue(panStartV - delta.y());
                break;
            }
            w = w->parentWidget();
        }
        return;
    }
    if (!drawing) return;
    int c = event->pos().x() / cellSize;
    int r = event->pos().y() / cellSize;
    if (r >= 0 && r < rows && c >= 0 && c < cols) {
        grid[r][c] = drawValue;
        update();
    }
}

void GameOfLifeWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    drawing = false;
    if (panning) panning = false;
}

void GameOfLifeWidget::loadPatternFromFile(const QString &path) {
    if (path.endsWith(".rle", Qt::CaseInsensitive)) {
        loadRLEFromFile(path);
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Can't open pattern file" << path;
        return;
    }
    QTextStream in(&f);
    QVector<QPair<int,int>> coords;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith('#')) continue;
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            bool ok1, ok2;
            int x = parts[0].toInt(&ok1);
            int y = parts[1].toInt(&ok2);
            if (ok1 && ok2) coords.append(qMakePair(x, y));
        }
    }
    if (coords.isEmpty()) return;

    // place pattern centered in the grid
    int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;
    for (auto &p : coords) {
        minx = qMin(minx, p.first);
        miny = qMin(miny, p.second);
        maxx = qMax(maxx, p.first);
        maxy = qMax(maxy, p.second);
    }
    int w = maxx - minx + 1;
    int h = maxy - miny + 1;
    int offsetR = (rows - h) / 2 - miny;
    int offsetC = (cols - w) / 2 - minx;

    // clear existing grid to avoid leftover cells interfering
    for (auto &row : grid) std::fill(row.begin(), row.end(), 0);

    QString placedCoords;
    for (auto &p : coords) {
        int r = p.second + offsetR;
        int c = p.first + offsetC;
        if (r >= 0 && r < rows && c >= 0 && c < cols) {
            grid[r][c] = 1;
            placedCoords += QString("(%1,%2) ").arg(c).arg(r);
        } else {
            QString msg = QString("Pattern coord out of bounds after offset: %1 %2 -> %3 %4").arg(p.first).arg(p.second).arg(c).arg(r);
            qDebug() << msg;
            if (loggingEnabled) logMessage(msg);
        }
    }
    QString info = QString("Loaded pattern %1 bounds: [%2,%3]-[%4,%5] placed at offsetC,offsetR: %6,%7 placedCoords:%8")
            .arg(path).arg(minx).arg(miny).arg(maxx).arg(maxy).arg(offsetC).arg(offsetR).arg(placedCoords);
    qDebug() << info;
    if (loggingEnabled) logMessage(info);
    if (loggingEnabled) {
        int alive0 = 0;
        for (int r = 0; r < rows; ++r) for (int c = 0; c < cols; ++c) alive0 += grid[r][c];
        logMessage(QString("Initial alive=%1").arg(alive0));
        dumpGridSnapshot("Initial placement");
    }
    update();
}

void GameOfLifeWidget::loadRLEFromFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Can't open RLE file" << path;
        return;
    }
    QTextStream in(&f);
    QString headerLine;
    QString data;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().isEmpty()) continue;
        if (line.startsWith('#')) continue;
        if (line.contains('x') && line.contains('y') && headerLine.isEmpty()) {
            headerLine = line;
            continue;
        }
        data += line.trimmed();
    }
    f.close();

    // parse RLE data
    int x = 0, y = 0;
    int run = 0;
    QVector<QPair<int,int>> coords;
    for (int i = 0; i < data.size(); ++i) {
        QChar ch = data[i];
        if (ch.isDigit()) {
            run = run * 10 + ch.digitValue();
            continue;
        }
        int count = (run == 0) ? 1 : run;
        run = 0;
        if (ch == 'b') {
            x += count;
        } else if (ch == 'o') {
            for (int k = 0; k < count; ++k) {
                coords.append(qMakePair(x, y));
                x++;
            }
        } else if (ch == '$') {
            y += count;
            x = 0;
        } else if (ch == '!') {
            break;
        }
    }

    if (coords.isEmpty()) return;

    // compute bounds
    int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;
    for (auto &p : coords) {
        minx = std::min(minx, p.first);
        miny = std::min(miny, p.second);
        maxx = std::max(maxx, p.first);
        maxy = std::max(maxy, p.second);
    }
    int w = maxx - minx + 1;
    int h = maxy - miny + 1;
    int offsetR = (rows - h) / 2 - miny;
    int offsetC = (cols - w) / 2 - minx;

    for (auto &row : grid) std::fill(row.begin(), row.end(), 0);
    QString placedCoords;
    for (auto &p : coords) {
        int rr = p.second + offsetR;
        int cc = p.first + offsetC;
        if (rr >= 0 && rr < rows && cc >= 0 && cc < cols) {
            grid[rr][cc] = 1;
            placedCoords += QString("(%1,%2) ").arg(cc).arg(rr);
        } else {
            QString msg = QString("RLE coord out of bounds after offset: %1 %2 -> %3 %4").arg(p.first).arg(p.second).arg(cc).arg(rr);
            qDebug() << msg;
            if (loggingEnabled) logMessage(msg);
        }
    }
    QString info = QString("Loaded RLE %1 bounds: [%2,%3]-[%4,%5] placed at offsetC,offsetR: %6,%7 placedCoords:%8")
            .arg(path).arg(minx).arg(miny).arg(maxx).arg(maxy).arg(offsetC).arg(offsetR).arg(placedCoords);
    qDebug() << info;
    if (loggingEnabled) {
        logMessage(info);
        int alive0 = 0;
        for (int r = 0; r < rows; ++r) for (int c = 0; c < cols; ++c) alive0 += grid[r][c];
        logMessage(QString("Initial alive=%1").arg(alive0));
        dumpGridSnapshot("Initial placement");
    }
    update();
}

void GameOfLifeWidget::dumpGridSnapshot(const QString &label) const {
    QFile f("gameoflife_debug.log");
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream ts(&f);
    ts << "-- " << label << " --\n";
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            ts << (grid[r][c] ? QChar('#') : QChar('.'));
        }
        ts << '\n';
    }
    ts << "-- end " << label << " --\n";
    f.close();
}

void GameOfLifeWidget::shiftGrid(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    std::vector<std::vector<uint8_t>> newGrid(rows, std::vector<uint8_t>(cols, 0));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!grid[r][c]) continue;
            int nr = r + dy;
            int nc = c + dx;
            if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) newGrid[nr][nc] = 1;
        }
    }
    grid.swap(newGrid);
    nextGrid = grid;
    update();
}

void GameOfLifeWidget::expandIfNeeded() {
    bool expanded = false;
    // check top row
    while (rows > 0) {
        bool topAlive = false;
        for (int c = 0; c < cols; ++c) if (grid[0][c]) { topAlive = true; break; }
        if (!topAlive) break;
        grid.insert(grid.begin(), std::vector<uint8_t>(cols, 0));
        nextGrid.insert(nextGrid.begin(), std::vector<uint8_t>(cols, 0));
        rows++;
        expanded = true;
    }

    // check bottom row
    while (rows > 0) {
        bool bottomAlive = false;
        for (int c = 0; c < cols; ++c) if (grid[rows-1][c]) { bottomAlive = true; break; }
        if (!bottomAlive) break;
        grid.push_back(std::vector<uint8_t>(cols, 0));
        nextGrid.push_back(std::vector<uint8_t>(cols, 0));
        rows++;
        expanded = true;
    }

    // check left column
    while (cols > 0) {
        bool leftAlive = false;
        for (int r = 0; r < rows; ++r) if (grid[r][0]) { leftAlive = true; break; }
        if (!leftAlive) break;
        for (int r = 0; r < rows; ++r) grid[r].insert(grid[r].begin(), 0);
        for (int r = 0; r < rows; ++r) nextGrid[r].insert(nextGrid[r].begin(), 0);
        cols++;
        expanded = true;
    }

    // check right column
    while (cols > 0) {
        bool rightAlive = false;
        for (int r = 0; r < rows; ++r) if (grid[r][cols-1]) { rightAlive = true; break; }
        if (!rightAlive) break;
        for (int r = 0; r < rows; ++r) grid[r].push_back(0);
        for (int r = 0; r < rows; ++r) nextGrid[r].push_back(0);
        cols++;
        expanded = true;
    }

    if (expanded) {
        setMinimumSize(cols * cellSize, rows * cellSize);
        // ensure the widget actual size updates so enclosing QScrollArea updates scrollbars
        resize(cols * cellSize, rows * cellSize);
        updateGeometry();
        qDebug() << "Grid expanded to" << cols << "x" << rows;
        if (loggingEnabled) logMessage(QString("Grid expanded to %1 x %2").arg(cols).arg(rows));
    }
}

void GameOfLifeWidget::setLoggingEnabled(bool enabled) {
    loggingEnabled = enabled;
    if (loggingEnabled) logMessage("Logging enabled");
    else logMessage("Logging disabled");
}

void GameOfLifeWidget::logMessage(const QString &msg) const {
    QFile f("gameoflife_debug.log");
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream ts(&f);
    ts << QDateTime::currentDateTime().toString(Qt::ISODate) << " - " << msg << "\n";
    f.close();
}

void GameOfLifeWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space) {
        toggleRunning();
    } else if (event->key() == Qt::Key_C) {
        clear();
    } else if (event->key() == Qt::Key_R) {
        randomize();
    } else if (event->key() == Qt::Key_S) {
        step();
    }
}
