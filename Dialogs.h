#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

struct Inst {
    QString id, name, mcVer, loader, loaderVer, group;
};

class AddInstDialog : public QDialog {
public:
    QString outName, outMc, outLoader, outLoaderVer, outGroup;
    AddInstDialog(QNetworkAccessManager *nam, QWidget *parent)
        : QDialog(parent), net(nam) {
        setWindowTitle("Add Instance");
        resize(500, 360);
        auto *lay = new QVBoxLayout(this);

        auto *hName = new QHBoxLayout();
        hName->addWidget(new QLabel("Name:"));
        edName = new QLineEdit("1.21.4");
        hName->addWidget(edName);
        lay->addLayout(hName);

        auto *hGrp = new QHBoxLayout();
        hGrp->addWidget(new QLabel("Group:"));
        edGroup = new QLineEdit("Ungrouped");
        hGrp->addWidget(edGroup);
        lay->addLayout(hGrp);

        lay->addWidget(new QLabel("Minecraft Version:"));
        cbMc = new QComboBox();
        cbMc->addItems({"1.21.4", "1.21.1", "1.20.4", "1.20.1", "1.18.2", "1.16.5", "1.12.2"});
        lay->addWidget(cbMc);

        auto *rep = net->get(QNetworkRequest(QUrl(
            "https://mojang.com")));
        connect(rep, &QNetworkReply::finished, [this, rep]() {
            if (rep->error() == QNetworkReply::NoError) {
                auto arr = QJsonDocument::fromJson(rep->readAll()).object()["versions"].toArray();
                if (!arr.isEmpty()) {
                    cbMc->clear();
                    for (int i = 0; i < arr.size() && i < 50; ++i) {
                        auto o = arr[i].toObject();
                        if (o["type"].toString() == "release") cbMc->addItem(o["id"].toString());
                    }
                }
            }
            rep->deleteLater();
        });

        lay->addWidget(new QLabel("Mod Loader:"));
        auto *hLoad = new QHBoxLayout();
        grpLoader = new QButtonGroup(this);
        QStringList loaders = {"Vanilla", "Fabric", "Forge", "NeoForge", "Quilt"};
        for (int i = 0; i < loaders.size(); ++i) {
            auto *rb = new QRadioButton(loaders[i]);
            if (i == 0) rb->setChecked(true);
            grpLoader->addButton(rb, i);
            hLoad->addWidget(rb);
        }
        lay->addLayout(hLoad);

        cbLoaderVer = new QComboBox();
        cbLoaderVer->addItem("None");
        lay->addWidget(cbLoaderVer);

        auto upd = [this]() {
            QString mc = cbMc->currentText();
            QString ld = grpLoader->checkedButton()->text();
            edName->setText(ld == "Vanilla" ? mc : (ld + " " + mc));
            fetchLoader(mc, ld);
        };
        connect(cbMc, &QComboBox::currentTextChanged, upd);
        connect(grpLoader, &QButtonGroup::idClicked, upd);

        auto *hBtns = new QHBoxLayout();
        auto *bOk = new QPushButton("OK");
        auto *bCancel = new QPushButton("Cancel");
        bOk->setStyleSheet("background:#2B7CD3;color:#fff;padding:8px 20px;font-weight:bold;");
        connect(bOk, &QPushButton::clicked, [this]() {
            outName = edName->text().trimmed();
            outMc = cbMc->currentText();
            outLoader = grpLoader->checkedButton()->text();
            outLoaderVer = cbLoaderVer->currentText();
            outGroup = edGroup->text().trimmed();
            if (outGroup.isEmpty()) outGroup = "Ungrouped";
            if (!outName.isEmpty()) accept();
        });
        connect(bCancel, &QPushButton::clicked, this, &QDialog::reject);
        hBtns->addStretch();
        hBtns->addWidget(bOk);
        hBtns->addWidget(bCancel);
        lay->addLayout(hBtns);
    }

private:
    QNetworkAccessManager *net;
    QLineEdit *edName, *edGroup;
    QComboBox *cbMc, *cbLoaderVer;
    QButtonGroup *grpLoader;

    void fetchLoader(const QString &mc, const QString &ld) {
        cbLoaderVer->clear();
        if (ld == "Vanilla") { cbLoaderVer->addItem("None"); return; }
        cbLoaderVer->addItem("Loading...");
        QUrl url;
        if (ld == "Fabric") url = QUrl("https://fabricmc.net" + mc);
        else if (ld == "Quilt") url = QUrl("https://quiltmc.org" + mc);
        else if (ld == "NeoForge") url = QUrl("https://neoforged.net");
        else if (ld == "Forge") url = QUrl("https://minecraftforge.net");

        auto *rep = net->get(QNetworkRequest(url));
        connect(rep, &QNetworkReply::finished, [this, rep, ld, mc]() {
            cbLoaderVer->clear();
            if (rep->error() == QNetworkReply::NoError) {
                auto doc = QJsonDocument::fromJson(rep->readAll());
                if (ld == "Fabric" || ld == "Quilt") {
                    for (int i = 0; i < doc.array().size() && i < 10; ++i)
                        cbLoaderVer->addItem(doc.array()[i].toObject()["loader"].toObject()["version"].toString());
                } else if (ld == "NeoForge") {
                    auto arr = doc.object()["versions"].toArray();
                    for (int i = arr.size() - 1; i >= 0 && cbLoaderVer->count() < 10; --i)
                        cbLoaderVer->addItem(arr[i].toString());
                } else if (ld == "Forge") {
                    auto pr = doc.object()["promos"].toObject();
                    if (pr.contains(mc + "-recommended")) cbLoaderVer->addItem(pr[mc + "-recommended"].toString());
                    if (pr.contains(mc + "-latest")) cbLoaderVer->addItem(pr[mc + "-latest"].toString());
                }
            }
            if (cbLoaderVer->count() == 0) cbLoaderVer->addItem("Official " + ld);
            rep->deleteLater();
        });
    }
};

class GameOverlayDialog : public QDialog {
public:
    GameOverlayDialog(const Inst &inst, QWidget *parent) : QDialog(parent) {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        showFullScreen();
        setStyleSheet("background:#101820; color:white;");
        auto *vMain = new QVBoxLayout(this);
        vMain->setContentsMargins(16, 12, 16, 12);

        auto *hTop = new QHBoxLayout();
        hTop->addWidget(new QLabel("Running: " + inst.name));
        hTop->addStretch();
        auto mkBtn = [](const QString &t, int w, int h) {
            auto *b = new QPushButton(t);
            b->setFixedSize(w, h);
            b->setStyleSheet("background:rgba(255,255,255,45);border-radius:8px;color:white;font-weight:bold;");
            return b;
        };
        auto *bEsc = mkBtn("||", 44, 44);
        connect(bEsc, &QPushButton::clicked, this, &QDialog::accept);
        hTop->addWidget(mkBtn("F5", 44, 44));
        hTop->addWidget(mkBtn("T", 44, 44));
        hTop->addWidget(bEsc);
        vMain->addLayout(hTop);

        vMain->addStretch();
        auto *hint = new QLabel("Нажмите || вверху справа для выхода");
        hint->setAlignment(Qt::AlignCenter);
        vMain->addWidget(hint);
        vMain->addStretch();

        auto *hBot = new QHBoxLayout();
        auto *pad = new QGridLayout();
        pad->addWidget(mkBtn("W", 50, 50), 0, 1);
        pad->addWidget(mkBtn("A", 50, 50), 1, 0);
        pad->addWidget(mkBtn("S", 50, 50), 1, 1);
        pad->addWidget(mkBtn("D", 50, 50), 1, 2);
        hBot->addLayout(pad);
        hBot->addStretch();

        auto *hBar = new QHBoxLayout();
        for (int i = 1; i <= 9; ++i) hBar->addWidget(mkBtn(QString::number(i), 34, 34));
        hBar->addWidget(mkBtn("...", 34, 34));
        hBot->addLayout(hBar);
        hBot->addStretch();

        auto *vAct = new QVBoxLayout();
        vAct->addWidget(mkBtn("↑", 50, 50));
        vAct->addWidget(mkBtn("↓", 50, 50));
        hBot->addLayout(vAct);
        vMain->addLayout(hBot);
    }
};
