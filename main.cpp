#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "StorageManager.h"
#include "AccountManager.h"
#include "SkinManager.h"
#include "VersionPicker.h"
#include "SetupWizard.h"
#include "GameLauncher.h"

struct Inst { QString id, name, mc, ld, rend; };

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QMainWindow w;
    w.setWindowTitle("Prism Launcher");
    w.setStyleSheet("background:#1E1E1E;color:#FFF;");

    QString dir = "/storage/emulated/0/PrismLauncher";
    auto *net = new QNetworkAccessManager(&w);
    auto *gl = new GameLauncher(net, &w);
    QList<Inst> list;
    AccountInfo curAcc{"Player", "Offline", AccountManager::makeOfflineUuid("Player"), "0"};

    auto *cw = new QWidget(&w);
    auto *vl = new QVBoxLayout(cw);
    auto *hl = new QHBoxLayout();
    auto *bAdd = new QPushButton("+ Add Instance");
    auto *bDel = new QPushButton("Delete");
    auto *bAcc = new QPushButton("Accounts (" + curAcc.name + ")");
    auto *bSkin = new QPushButton("Skin");
    auto *bRun = new QPushButton("> Launch");
    bRun->setStyleSheet("background:#2B7CD3;font-weight:bold;padding:8px;");

    hl->addWidget(bAdd);
    hl->addWidget(bDel);
    hl->addStretch();
    hl->addWidget(bAcc);
    hl->addWidget(bSkin);
    hl->addWidget(bRun);
    vl->addLayout(hl);

    auto *lw = new QListWidget();
    lw->setStyleSheet("background:#252525;font-size:15px;");
    vl->addWidget(lw);
    w.setCentralWidget(cw);

    auto save = [&]() {
        QJsonArray arr;
        for (auto &i : list) {
            QJsonObject o; o["id"]=i.id; o["n"]=i.name; o["m"]=i.mc; o["l"]=i.ld; o["r"]=i.rend;
            arr.append(o);
        }
        QFile f(dir + "/instances.json");
        if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(arr).toJson()); f.close(); }
    };

    auto redraw = [&]() {
        lw->clear();
        for (auto &i : list) lw->addItem(i.name + "  [" + i.ld + " | " + i.rend + "]");
    };

    auto load = [&]() {
        list.clear();
        QFile f(dir + "/instances.json");
        if (f.open(QIODevice::ReadOnly)) {
            for (auto v : QJsonDocument::fromJson(f.readAll()).array()) {
                auto o = v.toObject();
                QString r = o["r"].toString();
                list.append({o["id"].toString(), o["n"].toString(), o["m"].toString(),
                             o["l"].toString(), r.isEmpty() ? "Holy GL4ES" : r});
            }
            f.close();
        }
        auto accs = AccountManager::load(dir);
        if (!accs.isEmpty()) {
            curAcc = accs.first();
            bAcc->setText("Accounts (" + curAcc.name + ")");
        }
        redraw();
    };

    w.show();

    QTimer::singleShot(250, [&]() {
        if (StorageManager::ensureReady(dir, &w)) {
            if (!SetupWizardDialog::isCompleted(dir)) {
                SetupWizardDialog wizard(dir, net, &w);
                wizard.exec();
            }
            load();
        }
    });

    QObject::connect(bAcc, &QPushButton::clicked, [&]() {
        if (!StorageManager::ensureReady(dir, &w)) return;
        curAcc = AccountManager::openDialog(dir, net, &w);
        bAcc->setText("Accounts (" + curAcc.name + ")");
    });

    QObject::connect(bSkin, &QPushButton::clicked, [&]() {
        if (!StorageManager::ensureReady(dir, &w)) return;
        SkinManager::openSkinDialog(dir, curAcc, net, &w);
    });

    QObject::connect(bAdd, &QPushButton::clicked, [&]() {
        if (!StorageManager::ensureReady(dir, &w)) return;
        if (list.isEmpty()) load();

        VersionPickerDialog dlg(net, &w);
        if (dlg.exec() == QDialog::Accepted) {
            QString mc = dlg.selectedVer;
            QString ld = dlg.selectedLoader;
            QString rn = dlg.selectedRenderer;
            QString id = ld + "_" + mc + "_" + QString::number(QDateTime::currentSecsSinceEpoch());
            QDir().mkpath(dir + "/instances/" + id + "/.minecraft/mods");
            list.append({id, ld + " " + mc, mc, ld, rn});
            save(); redraw();
            gl->prepareAndLaunch({id, ld + " " + mc, mc, ld, "latest",
                                  curAcc.name, curAcc.type, curAcc.uuid, curAcc.token, dir, 2048, rn}, &w);
        }
    });

    QObject::connect(bDel, &QPushButton::clicked, [&]() {
        if (!StorageManager::ensureReady(dir, &w)) return;
        int r = lw->currentRow();
        if (r >= 0) {
            QDir(dir + "/instances/" + list[r].id).removeRecursively();
            list.removeAt(r); save(); redraw();
        }
    });

    QObject::connect(bRun, &QPushButton::clicked, [&]() {
        if (!StorageManager::ensureReady(dir, &w)) return;
        int r = lw->currentRow();
        if (r >= 0) {
            gl->prepareAndLaunch({list[r].id, list[r].name, list[r].mc, list[r].ld,
                                  "latest", curAcc.name, curAcc.type, curAcc.uuid, curAcc.token, dir, 2048, list[r].rend}, &w);
        }
    });

    return a.exec();
}
