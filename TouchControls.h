#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QCoreApplication>

class TouchOverlay : public QDialog {
public:
    explicit TouchOverlay(const QString &versionName, QWidget *parent = nullptr)
        : QDialog(parent) {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        showFullScreen();
        setStyleSheet("background:#101820; color:white;");

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(16, 12, 16, 12);

        // 1. ВЕРХНЯЯ ПАНЕЛЬ (Слева пусто как на схеме, справа F5, T, Esc)
        auto *topRow = new QHBoxLayout();
        topRow->addStretch();

        auto *btnF5 = makeBtn("🎥", 46, 46, "F5", Qt::Key_F5);
        auto *btnT  = makeBtn("💬", 46, 46, "T", Qt::Key_T);
        auto *btnEsc = makeBtn("||", 46, 46, "Esc", Qt::Key_Escape);

        connect(btnEsc, &QPushButton::clicked, this, &QDialog::accept);

        topRow->addWidget(btnF5);
        topRow->addSpacing(8);
        topRow->addWidget(btnT);
        topRow->addSpacing(8);
        topRow->addWidget(btnEsc);
        root->addLayout(topRow);

        root->addStretch();

        // Инфо по центру (пока не подключен 3D-рендер)
        auto *centerLbl = new QLabel(
            "Minecraft " + versionName + "\n"
            "Сенсорное управление активно (нажмите || вверху справа для выхода)");
        centerLbl->setAlignment(Qt::AlignCenter);
        centerLbl->setStyleSheet("color:#667788; font-size:14px;");
        root->addWidget(centerLbl);

        root->addStretch();

        // 2. НИЖНЯЯ ПАНЕЛЬ (Слева WASD, По центру 1..9 + E, Справа Пробел и SHIFT)
        auto *botRow = new QHBoxLayout();

        // Крестовина WASD (как на вашей схеме: W сверху, S снизу, A/D по бокам)
        auto *dpad = new QGridLayout();
        dpad->setSpacing(4);
        dpad->addWidget(makeBtn("W", 54, 54, "W", Qt::Key_W), 0, 1);
        dpad->addWidget(makeBtn("A", 54, 54, "A", Qt::Key_A), 1, 0);
        auto *centerDot = new QLabel("•");
        centerDot->setAlignment(Qt::AlignCenter);
        centerDot->setStyleSheet("background:rgba(255,255,255,25);border-radius:8px;");
        dpad->addWidget(centerDot, 1, 1);
        dpad->addWidget(makeBtn("D", 54, 54, "D", Qt::Key_D), 1, 2);
        dpad->addWidget(makeBtn("S", 54, 54, "S", Qt::Key_S), 2, 1);
        botRow->addLayout(dpad);

        botRow->addStretch();

        // Хотбар 1-9 и кнопка инвентаря E (...)
        auto *hotbar = new QHBoxLayout();
        hotbar->setSpacing(3);
        Qt::Key numKeys[9] = {
            Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5,
            Qt::Key_6, Qt::Key_7, Qt::Key_8, Qt::Key_9
        };
        for (int i = 0; i < 9; ++i) {
            hotbar->addWidget(makeBtn(QString::number(i + 1), 38, 38,
                                      QString::number(i + 1), numKeys[i]));
        }
        hotbar->addWidget(makeBtn("...", 38, 38, "E", Qt::Key_E));
        botRow->addLayout(hotbar, 0);

        botRow->addStretch();

        // Справа: Прыжок (Пробел) сверху, Приседание (SHIFT) снизу
        auto *actions = new QVBoxLayout();
        actions->setSpacing(10);
        actions->addWidget(makeBtn("↑", 58, 58, "Space", Qt::Key_Space));
        actions->addWidget(makeBtn("↓", 58, 58, "Shift", Qt::Key_Shift));
        botRow->addLayout(actions);

        root->addLayout(botRow);
    }

private:
    QPushButton* makeBtn(const QString &label, int w, int h,
                         const QString &tip, Qt::Key key) {
        auto *btn = new QPushButton(label);
        btn->setFixedSize(w, h);
        btn->setToolTip(tip);
        btn->setStyleSheet(
            "QPushButton { background:rgba(255,255,255,40); border:1px solid rgba(255,255,255,80);"
            "border-radius:8px; color:white; font-weight:bold; font-size:15px; }"
            "QPushButton:pressed { background:rgba(43,124,211,180); }");

        // Передача нажатия и отпускания клавиши в движок
        connect(btn, &QPushButton::pressed, [this, key]() {
            QKeyEvent ev(QEvent::KeyPress, key, Qt::NoModifier);
            QCoreApplication::sendEvent(this, &ev);
        });
        connect(btn, &QPushButton::released, [this, key]() {
            QKeyEvent ev(QEvent::KeyRelease, key, Qt::NoModifier);
            QCoreApplication::sendEvent(this, &ev);
        });
        return btn;
    }
};
