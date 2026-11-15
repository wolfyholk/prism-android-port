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
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>

struct Inst {
    QString id;
    QString name;
    QString mcVer;
    QString loader;
    QString loaderVer;
    QString group;
};

// Генератор оригинальной иконки Prism Launcher (как на картинке)
static QPixmap makePrismIcon(int sz) {
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    double cx = sz / 2.0, cy = sz / 2.0, r = sz * 0.45;
    QColor cols[8] = {
        QColor("#DF5B6D"), QColor("#F48A5A"), QColor("#F5DF65"),
        QColor("#78B88E"), QColor("#4C82C4"), QColor("#6C488E"),
        QColor("#574336"), QColor("#7E5538")
    };
    // Верхний левый зеленый сектор и остальные 8 сегментов
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 8; ++i) {
        p.setBrush(cols[i]);
        double a1 = -90 + i * 37.5;
        p.drawPie(QRectF(cx - r, cy - r, r * 2, r * 2),
                  int(-a1 * 16), int(-37.5 * 16));
    }
    p.setBrush(QColor("#92CD5E"));
    p.drawPie(QRectF(cx - r, cy - r, r * 2, r * 2),
              int(90 * 16), int(60 * 16));
    // Белый внутренний куб
    double ir = r * 0.48;
    QPolygonF topF, leftF, rightF;
    topF << QPointF(cx, cy - ir)
         << QPointF(cx + ir * 0.86, cy - ir * 0.5)
         << QPointF(cx, cy)
         << QPointF(cx - ir * 0.86, cy - ir * 0.5);
    leftF << QPointF(cx - ir * 0.86, cy - ir * 0.5)
          << QPointF(cx, cy)
          << QPointF(cx, cy + ir)
          << QPointF(cx - ir * 0.86, cy + ir * 0.5);
    rightF << QPointF(cx, cy)
           << QPointF(cx + ir * 0.86, cy - ir * 0.5)
           << QPointF(cx + ir * 0.86, cy + ir * 0.5)
           << QPointF(cx, cy + ir);
    p.setBrush(QColor("#FFFFFF")); p.drawPolygon(topF);
    p.setBrush(QColor("#DCDCDC")); p.drawPolygon(leftF);
    p.setBrush(QColor("#EAEAEA")); p.drawPolygon(rightF);
    return pm;
}

// Диалог создания сборки с официальными версиями и загрузчиками
class AddInstDialog : public QDialog {
public:
    QString outName, outMc, outLoader, outLoaderVer, outGroup;

    AddInstDialog(QNetworkAccessManager *nam, QWidget *parent)
        : QDialog(parent), net(nam) {
        setWindowTitle("Add Instance - Prism Launcher");
        resize(560, 420);
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

        lay->addWidget(new QLabel("Minecraft Version (Official Mojang):"));
        cbMc = new QComboBox();
        cbMc->addItems({"1.21.4", "1.21.1", "1.20.4", "1.20.1",
                        "1.19.2", "1.18.2", "1.16.5", "1.12.2"});
        lay->addWidget(cbMc);

        // Загрузка реального списка версий с серверов Mojang
        QNetworkRequest req(QUrl(
            "https://mojang.com"));
        auto *rep = net->get(req);
        connect(rep, &QNetworkReply::finished, [this, rep]() {
            if (rep->error() == QNetworkReply::NoError) {
                auto doc = QJsonDocument::fromJson(rep->readAll());
                auto arr = doc.object()["versions"].toArray();
                if (!arr.isEmpty()) {
                    cbMc->clear();
                    for (int i = 0; i < arr.size() && i < 60; ++i) {
                        auto obj = arr[i].toObject();
                        if (obj["type"].toString() == "release") {
                            cbMc->addItem(obj["id"].toString());
                        }
                    }
                }
            }
            rep->deleteLater();
        });

        lay->addWidget(new QLabel("Mod Loader (Original):"));
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
        cbLoaderVer->addItem("None (Vanilla)");
        lay->addWidget(cbLoaderVer);

        auto updateNameAndLoader = [this]() {
            QString mc = cbMc->currentText();
            QString ld = grpLoader->checkedButton()->text();
            edName->setText(ld == "Vanilla" ? mc : (ld + " " + mc));
            fetchLoaderVersions(mc, ld);
        };

        connect(cbMc, &QComboBox::currentTextChanged, updateNameAndLoader);
        connect(grpLoader, &QButtonGroup::idClicked, updateNameAndLoader);

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

    void fetchLoaderVersions(const QString &mc, const QString &ld) {
        cbLoaderVer->clear();
        if (ld == "Vanilla") {
            cbLoaderVer->addItem("None");
            return;
        }
        cbLoaderVer->addItem("Loading...");
        QUrl url;
        if (ld == "Fabric") {
            url = QUrl("https://fabricmc.net" + mc);
        } else if (ld == "Quilt") {
            url = QUrl("https://quiltmc.org" + mc);
        } else if (ld == "NeoForge") {
            url = QUrl("https://neoforged.net");
        } else if (ld == "Forge") {
            url = QUrl("https://minecraftforge.net");
        }

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
                    for (int i = arr.size() - 1; i >= 0 && cbLoaderVer->count() < 15; --i) {
                        cbLoaderVer->addItem(arr[i].toString());
                    }
                } else if (ld == "Forge") {
                    auto promos = doc.object()["promos"].toObject();
                    if (promos.contains(mc + "-recommended"))
                        cbLoaderVer->addItem(promos[mc + "-recommended"].toString());
                    if (promos.contains(mc + "-latest"))
                        cbLoaderVer->addItem(promos[mc + "-latest"].toString());
                }
            }
            if (cbLoaderVer->count() == 0)
                cbLoaderVer->addItem("Official " + ld + " (" + mc + ")");
            rep->deleteLater();
        });
    }
};

// Игровой оверлей с сенсорным управлением как на вашей схеме
class GameOverlayDialog : public QDialog {
public:
    GameOverlayDialog(const Inst &inst, QWidget *parent) : QDialog(parent) {
        setWindowTitle("Minecraft " + inst.mcVer);
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        showFullScreen();
        setStyleSheet("background:#101820; color:white;");

        auto *vMain = new QVBoxLayout(this);
        vMain->setContentsMargins(16, 12, 16, 12);

        // Верхняя панель (F5, Чат T, Пауза Esc) справа вверху
        auto *hTop = new QHBoxLayout();
        auto *info = new QLabel("Running: " + inst.name + " [" + inst.loader + "]");
        info->setStyleSheet("color:#8899A6; font-size:12px;");
        hTop->addWidget(info);
        hTop->addStretch();

        auto mkTopBtn = [](const QString &t) {
            auto *b = new QPushButton(t);
            b->setFixedSize(44, 44);
            b->setStyleSheet("background:rgba(255,255,255,40);border-radius:8px;color:white;font-weight:bold;");
            return b;
        };
