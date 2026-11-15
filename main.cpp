#include <QApplication>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    QVBoxLayout *layout = new QVBoxLayout(&window);

    QLabel *title = new QLabel("Prism Launcher Android (Тест сборки)");
    title->setAlignment(Qt::AlignCenter);

    QPushButton *playBtn = new QPushButton("Запустить Minecraft");
    playBtn->setMinimumHeight(60);

    layout->addWidget(title);
    layout->addWidget(playBtn);

    window.show();
    return app.exec();
}
