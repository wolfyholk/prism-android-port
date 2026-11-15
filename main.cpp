#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QStyleFactory>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPainter>
#include <QPixmap>
#include "GameLauncher.h"

struct Inst {
    QString id, name, mcVer, loader, loaderVer, group;
};

static QPixmap makePrismIcon(int sz) {
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    double cx = sz / 2.0, cy = sz / 2.0, r = sz * 0.45;
    QColor cols = {
        QColor("#DF5B6D"), QColor("#F48A5A"), QColor("#F5DF65"),
        QColor("#78B88E"), QColor("#4C82C4"), QColor("#6C488E"),
        QColor("#574336"), QColor("#7E5538")
    };
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 8; ++i) {
        p.setBrush(cols[i]);
        p.drawPie(QRectF(cx - r, cy - r, r * 2, r * 2),
                  int(-(-90 + i * 37.5) * 16), int(-37.5 * 16));
    }
    p.setBrush(QColor("#92CD5E"));
    p.drawPie(QRectF(cx - r, cy - r, r * 2, r * 2), 90 * 16, 60 * 16);
    double ir = r * 0.48;
    QPolygonF tF, lF, rF;
    tF << QPointF(cx, cy - ir) << QPointF(cx + ir * 0.86, cy - ir * 0.5)
       << QPointF(cx, cy) << QPointF(cx - ir * 0.86, cy - ir * 0.5);
    lF << QPointF(cx - ir * 0.86, cy - ir * 0.5) << QPointF(cx, cy)
       << QPointF(cx, cy + ir) << QPointF(cx - ir * 0.86, cy + ir * 0.5);
    rF << QPointF(cx, cy) << QPointF(cx + ir * 0.86, cy - ir * 0.5)
       << QPointF(cx + ir * 0.86, cy + ir * 0.5) << QPointF(cx, cy + ir);
    p.setBrush(QColor("#FFFFFF")); p.drawPolygon(tF);
    p.setBrush(QColor("#DCDCDC")); p.drawPolygon(lF);
    p.setBrush(QColor("#EAEAEA")); p.drawPolygon(rF);
    return pm;
}

class AddInstDialog : public QDialog {
public:
    QString outName, outMc, outLoader, outLoaderVer, outGroup;
    AddInstDialog(QNetworkAccessManager *nam, QWidget *parent)
        : QDialog(parent), net(nam) {
        setWindowTitle("Add Instance");
        resize(520, 380);
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
        cbMc->addItems({"1.21.4", "1.21.1", "1.20.4", "1.20.1", "1.19.2", "1.18.2", "1.16.5", "1.12.2"});
        lay->addWidget(cbMc);

        QUrl mUrl("https://mojang.com");
        auto *rep = net->get(QNetworkRequest(mUrl));
        connect(rep, &QNetworkReply::finished, [this, rep]() {
            if (rep->error() == QNetworkReply::NoError) {
                auto arr = QJsonDocument::fromJson(rep->readAll()).object()["versions"].toArray();
                if (!arr.isEmpty()) {
                    cbMc->clear();
                    for (int i = 0; i < arr.size() && i < 60; ++i) {
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

        lay->addWidget(new QLabel("Loader Version:"));
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
                    auto arr = doc.array();
                    for (int i = 0; i < arr.size() && i < 15; ++i) {
                        auto v = arr[i].toObject()["loader"].toObject()["version"].toString();
                        if (!v.isEmpty()) cbLoaderVer->addItem(v);
                    }
                } else if (ld == "NeoForge") {
                    auto arr = doc.object()["versions"].toArray();
                    for (int i = arr.size() - 1; i >= 0 && cbLoaderVer->count() < 15; --i)
                        cbLoaderVer->addItem(arr[i].toString());
                } else if (ld == "Forge") {
                    auto promos = doc.object()["promos"].toObject();
                    if (promos.contains(mc + "-recommended")) cbLoaderVer->addItem(promos[mc + "-recommended"].toString());
                    if (promos.contains(mc + "-latest")) cbLoaderVer->addItem(promos[mc + "-latest"].toString());
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
        auto *hint = new QLabel("Экран игры (Нажмите || вверху справа для выхода)");
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
