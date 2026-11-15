#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "GameLauncher.h"

struct Inst { QString id, name, mc, ld; };

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QMainWindow w;
    w.setWindowTitle("Prism Launcher");
    w.setStyleSheet("background:#1E1E1E;color:#FFF;");

    auto *net = new QNetworkAccessManager(&w);
    auto *gl = new GameLauncher(net, &w);
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir + "/instances");
    QList<Inst> list;

    auto *cw = new QWidget(&w);
    auto *vl = new QVBoxLayout(cw);
    auto *hl = new QHBoxLayout();
    auto *bAdd = new QPushButton("+ Add Instance");
    auto *bDel = new QPushButton("Delete");
    auto *bRun = new QPushButton("> Launch");
    bRun->setStyleSheet("background:#2B7CD3;font-weight:bold;padding:8px;");
    hl->addWidget(bAdd); hl->addWidget(bDel); hl->addStretch(); hl->addWidget(bRun);
    vl->addLayout(hl);

    auto *lw = new QListWidget();
    lw->setStyleSheet("background:#252525;font-size:15px;");
    vl->addWidget(lw);
    w.setCentralWidget(cw);

    auto save = [&]() {
        QJsonArray arr;
        for (auto &i : list) {
            QJsonObject o; o["id"]=i.id; o["n"]=i.name; o["m"]=i.mc; o["l"]=i.ld;
            arr.append(o);
        }
        QFile f(dir + "/data.json");
        if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(arr).toJson()); f.close(); }
    };

    auto redraw = [&]() {
        lw->clear();
        for (auto &i : list) lw->addItem(i.name + "  [" + i.ld + " " + i.mc + "]");
    };

    QFile f(dir + "/data.json");
    if (f.open(QIODevice::ReadOnly)) {
        for (auto v : QJsonDocument::fromJson(f.readAll()).array()) {
            auto o = v.toObject();
            list.append({o["id"].toString(), o["n"].toString(), o["m"].toString(), o["l"].toString()});
        }
        f.close(); redraw();
    }

    QObject::connect(bAdd, &QPushButton::clicked, [&]() {
        QString mc = QInputDialog::getText(&w, "Add", "Версия (напр. 1.21.4):", QLineEdit::Normal, "1.21.4");
        if (mc.isEmpty()) return;
        QStringList lds = {"Vanilla", "Fabric", "Forge", "NeoForge", "Quilt"};
        QString ld = QInputDialog::getItem(&w, "Loader", "Загрузчик:", lds, 0, false);
        if (ld.isEmpty()) return;
        QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
        list.append({id, ld + " " + mc, mc, ld});
        save(); redraw();
    });

    QObject::connect(bDel, &QPushButton::clicked, [&]() {
        int r = lw->currentRow();
        if (r >= 0) { list.removeAt(r); save(); redraw(); }
    });

    QObject::connect(bRun, &QPushButton::clicked, [&]() {
        int r = lw->currentRow();
        if (r >= 0) gl->prepareAndLaunch({list[r].id, list[r].name, list[r].mc, list[r].ld, "latest", "Player", dir, 2048}, &w);
    });

    w.show();
    return a.exec();
}
