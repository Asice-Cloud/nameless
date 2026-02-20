#pragma once

#include <QWidget>
#include <QTimer>
#include <vector>

class GameOfLifeWidget : public QWidget {
    Q_OBJECT
public:
    explicit GameOfLifeWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override;

public slots:
    void step();
    void clear();
    void randomize();
    void toggleRunning();
    void setRunning(bool run);
    // load a pattern from a file (simple x y coordinate per line)
    void loadPatternFromFile(const QString &path);
    void loadRLEFromFile(const QString &path);
    void shiftGrid(int dx, int dy);
    void setLoggingEnabled(bool enabled);
    void dumpGridSnapshot(const QString &label) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    int cols;
    int rows;
    int cellSize;
    std::vector<std::vector<uint8_t>> grid;
    std::vector<std::vector<uint8_t>> nextGrid;
    QTimer timer;
    bool running;
    bool drawing;
    uint8_t drawValue;
    bool loggingEnabled;
    int generation;
    bool panning;
    QPoint panStartPos;
    int panStartH;
    int panStartV;

    int countNeighbors(int r, int c) const;
    void resizeGrid();
    void logMessage(const QString &msg) const;
    void expandIfNeeded();
};
