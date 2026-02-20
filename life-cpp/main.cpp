#include <QApplication>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include "GameOfLifeWidget.h"
#include <QScrollArea>
#include <QFileDialog>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Conway's Game of Life");

    auto *life = new GameOfLifeWidget;
    auto *scroll = new QScrollArea;
    scroll->setWidget(life);
    scroll->setWidgetResizable(false);

    auto *startBtn = new QPushButton("Start/Stop");
    auto *stepBtn = new QPushButton("Step");
    auto *clearBtn = new QPushButton("Clear");
    auto *randBtn = new QPushButton("Random");
    auto *loadBtn = new QPushButton("Load");
    auto *logBtn = new QPushButton("Log");
    logBtn->setCheckable(true);

    QObject::connect(startBtn, &QPushButton::clicked, life, &GameOfLifeWidget::toggleRunning);
    QObject::connect(stepBtn, &QPushButton::clicked, life, &GameOfLifeWidget::step);
    QObject::connect(clearBtn, &QPushButton::clicked, life, &GameOfLifeWidget::clear);
    QObject::connect(randBtn, &QPushButton::clicked, life, &GameOfLifeWidget::randomize);
    QObject::connect(loadBtn, &QPushButton::clicked, [&](){
        QString fn = QFileDialog::getOpenFileName(nullptr, "Load pattern", ":/", "Pattern files (*.txt);;All files (*)");
        if (!fn.isEmpty()) life->loadPatternFromFile(fn);
    });
    QObject::connect(logBtn, &QPushButton::toggled, [life](bool on){ life->setLoggingEnabled(on); });

    auto *hl = new QHBoxLayout;
    hl->addWidget(startBtn);
    hl->addWidget(stepBtn);
    hl->addWidget(clearBtn);
    hl->addWidget(randBtn);
    hl->addWidget(loadBtn);
    hl->addWidget(logBtn);
    hl->addStretch();

    auto *vl = new QVBoxLayout(&window);
    vl->addLayout(hl);
    vl->addWidget(scroll);

    window.setLayout(vl);
    window.show();
    // If AUTO_PATTERN environment variable is set, load and run a short debug sequence then exit.
    QByteArray autoPattern = qgetenv("AUTO_PATTERN");
    if (!autoPattern.isEmpty()) {
        QString fn = QString::fromLocal8Bit(autoPattern);
        life->setLoggingEnabled(true);
        life->loadPatternFromFile(fn);
        QByteArray autoShift = qgetenv("AUTO_SHIFT");
        if (!autoShift.isEmpty()) {
            QString s = QString::fromLocal8Bit(autoShift);
            QStringList parts = s.split(',');
            if (parts.size() >= 2) {
                bool ok1, ok2;
                int dx = parts[0].toInt(&ok1);
                int dy = parts[1].toInt(&ok2);
                if (ok1 && ok2) life->shiftGrid(dx, dy);
            }
        }
        // run a few steps to capture snapshots in the log
        for (int i = 0; i < 8; ++i) life->step();
        return 0;
    }

    return app.exec();
}
