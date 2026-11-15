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
#include <QStyleFactory>

struct Inst {
    QString name;
    QString ver;
    QString cat;
    QString col;
    QString tag;
};

class PrismWin : public QMainWindow {
public:
    PrismWin() {
        setWindowTitle("Prism Launcher 9.4");
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QPalette p;
        p.setColor(QPalette::Window, QColor(30, 30, 30));
        p.setColor(QPalette::WindowText, Qt::white);
        p.setColor(QPalette::Base, QColor(25, 25, 25));
        p.setColor(QPalette::Text, Qt::white);
        p.setColor(QPalette::Button, QColor(45, 45, 45));
        p.setColor(QPalette::ButtonText, Qt::white);
        qApp->setPalette(p);

        list = {
            {"Fabric 1.16.5", "1.16.5", "Ungrouped", "#36C5F0", "F"},
            {"Forge 1.12.2", "1.12.2", "Ungrouped", "#D93829", "TNT"},
            {"NeoForge 1.21.1", "1.21.1", "Ungrouped", "#E0E0E0", "NF"},
            {"Quilt 1.18.2", "1.18.2", "Ungrouped", "#F2C94C", "Q"},
            {"Snapshot 26.1", "26.1", "Ungrouped", "#795548", "26.1"},
            {"Vanilla 1.21.11", "1.21.11", "Ungrouped", "#6E7F80", "MC"},
            {"All the Mods 10", "1.21.1", "Modpack", "#F2994A", "ATM"},
            {"Better MC", "1.20.1", "Modpack", "#56CCF2", "BMC"},
            {"Cobblemon", "1.20.1", "Modpack", "#EB5757", "CB"},
            {"SkyFactory 4", "1.12.2", "Modpack", "#6FCF97", "SF4"},
            {"Pixelmon", "1.16.5", "Modpack", "#BB6BD9", "PM"},
            {"Additive", "1.21.4", "Vanilla-like", "#FF7675", "U"},
            {"Adrenaline", "1.21.4", "Vanilla-like", "#A3CB38", "AD"},
            {"Fabulously Opt.", "1.21.4", "Vanilla-like", "#C4B581", "FO"}
        };

        auto *root = new QWidget(this);
        auto *vMain = new QVBoxLayout(root);
        vMain->setContentsMargins(0, 0, 0, 0);
        vMain->setSpacing(0);

        // Верхняя панель
        auto *top = new QWidget();
        top->setFixedHeight(44);
        top->setStyleSheet("background:#2B2B2B;");
        auto *hTop = new QHBoxLayout(top);
        hTop->setContentsMargins(8, 4, 8, 4);

        auto *bAdd = topBtn("+ Add Instance");
        auto *bFld = topBtn("Folders");
        auto *bSet = topBtn("Settings");
        auto *bHlp = topBtn("Help");
        auto *bAcc = topBtn("Accounts");

        connect(bAdd, &QPushButton::clicked, [this]() {
            bool ok;
            QString n = QInputDialog::getText(
                this, "Add", "Name:",
                QLineEdit::Normal, "New 1.21.4", &ok);
            if (ok && !n.isEmpty()) {
                list.append({n, "1.21.4", "Ungrouped", "#55EFC4", "NEW"});
                buildGrid(n);
            }
        });
        connect(bSet, &QPushButton::clicked, [this]() {
            QMessageBox::information(this, "Settings", "Java & Render");
        });
        connect(bAcc, &QPushButton::clicked, [this]() {
            QMessageBox::information(this, "Accounts", "Microsoft / Offline");
        });

        hTop->addWidget(bAdd);
        hTop->addWidget(bFld);
        hTop->addWidget(bSet);
        hTop->addWidget(bHlp);
        hTop->addStretch();
        hTop->addWidget(bAcc);
        vMain->addWidget(top);

        // Центр
        auto *hMid = new QHBoxLayout();
        hMid->setContentsMargins(0, 0, 0, 0);
        hMid->setSpacing(0);

        auto *scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet("background:#1E1E1E;");

        gridBox = new QWidget();
        gridBox->setStyleSheet("background:#1E1E1E;");
        gridLay = new QVBoxLayout(gridBox);
        gridLay->setContentsMargins(12, 10, 12, 10);
        scroll->setWidget(gridBox);
        hMid->addWidget(scroll, 1);

        // Правая панель
        auto *side = new QWidget();
        side->setFixedWidth(180);
        side->setStyleSheet("background:#252525;");
        auto *vSide = new QVBoxLayout(side);
        vSide->setContentsMargins(10, 14, 10, 10);
        vSide->setSpacing(4);

        sIcon = new QLabel("MC");
        sIcon->setFixedSize(64, 64);
        sIcon->setAlignment(Qt::AlignCenter);
        sIcon->setStyleSheet("background:#6E7F80;border-radius:8px;color:#fff;font-weight:bold;");

        sName = new QLabel("Vanilla 1.21.11");
        sName->setAlignment(Qt::AlignCenter);
        sName->setWordWrap(true);
        sName->setStyleSheet("color:#EAEAEA;font-weight:bold;");

        vSide->addWidget(sIcon, 0, Qt::AlignHCenter);
        vSide->addWidget(sName);

        auto *bPlay = sideBtn("> Launch", true);
        auto *bKill = sideBtn("x Kill", false);
        bKill->setEnabled(false);

        connect(bPlay, &QPushButton::clicked, [this]() {
            QMessageBox::information(this, "Launch", "Start: " + sName->text());
        });

        vSide->addWidget(bPlay);
        vSide->addWidget(bKill);
        vSide->addWidget(sideBtn("Edit", false));
        vSide->addWidget(sideBtn("Change Group", false));
        vSide->addWidget(sideBtn("Folder", false));
        vSide->addWidget(sideBtn("Export", false));
        vSide->addWidget(sideBtn("Copy", false));
        vSide->addWidget(sideBtn("Delete", false));
        vSide->addWidget(sideBtn("Shortcut", false));
        vSide->addStretch();

        hMid->addWidget(side);
        vMain->addLayout(hMid, 1);

        // Низ
        auto *bot = new QWidget();
        bot->setFixedHeight(26);
        bot->setStyleSheet("background:#252525;");
        auto *hBot = new QHBoxLayout(bot);
        hBot->setContentsMargins(10, 0, 10, 0);

        stLeft = new QLabel("Minecraft 1.21.11");
        stLeft->setStyleSheet("color:#BBB;font-size:11px;");
        auto *stRight = new QLabel("Total playtime: 0s");
        stRight->setStyleSheet("color:#BBB;font-size:11px;");

        hBot->addWidget(stLeft);
        hBot->addStretch();
        hBot->addWidget(stRight);
        vMain->addWidget(bot);

        setCentralWidget(root);
        buildGrid("Vanilla 1.21.11");
    }

private:
    QList<Inst> list;
    QWidget *gridBox;
    QVBoxLayout *gridLay;
    QLabel *sIcon;
    QLabel *sName;
    QLabel *stLeft;

    QPushButton* topBtn(const QString &t) {
        auto *b = new QPushButton(t);
        b->setStyleSheet("background:transparent;color:#EAEAEA;border:none;padding:6px 8px;");
        return b;
    }

    QPushButton* sideBtn(const QString &t, bool main) {
        auto *b = new QPushButton(t);
        if (main) {
            b->setStyleSheet("text-align:left;background:#2E3436;color:#FFF;font-weight:bold;padding:8px;border-radius:4px;");
        } else {
            b->setStyleSheet("text-align:left;background:transparent;color:#EAEAEA;border:none;padding:6px 8px;");
        }
        return b;
    }

    void buildGrid(const QString &act) {
        QLayoutItem *c;
        while ((c = gridLay->takeAt(0)) != nullptr) {
            if (c->widget()) delete c->widget();
            delete c;
        }
        QStringList cats = {"Ungrouped", "Modpack", "Vanilla-like"};
        for (const QString &cat : cats) {
            auto *hdr = new QLabel("v  " + cat);
            hdr->setStyleSheet("color:#CCC;font-weight:bold;");
            gridLay->addWidget(hdr);

            auto *w = new QWidget();
            auto *g = new QGridLayout(w);
            g->setAlignment(Qt::AlignLeft);
            int col = 0, row = 0;

            for (const auto &it : list) {
                if (it.cat != cat) continue;
                bool sel = (it.name == act);
                auto *tile = new QPushButton();
                tile->setFixedSize(92, 98);
                auto *tl = new QVBoxLayout(tile);
                tl->setContentsMargins(4, 6, 4, 4);

                auto *ic = new QLabel(it.tag);
                ic->setFixedSize(48, 48);
                ic->setAlignment(Qt::AlignCenter);
                ic->setStyleSheet("background:" + it.col + ";border-radius:6px;color:#111;font-weight:bold;");

                auto *lb = new QLabel(it.name);
                lb->setAlignment(Qt::AlignCenter);
                lb->setWordWrap(true);
                lb->setStyleSheet(sel ? "background:#2B7CD3;color:#fff;font-size:11px;" : "color:#EAEAEA;font-size:11px;");

                tl->addWidget(ic, 0, Qt::AlignHCenter);
                tl->addWidget(lb);
                tile->setStyleSheet(sel ? "background:#223B59;border:1px solid #2B7CD3;border-radius:6px;" : "background:transparent;border:none;");

                connect(tile, &QPushButton::clicked, [this, it]() {
                    sName->setText(it.name);
                    sIcon->setText(it.tag);
                    sIcon->setStyleSheet("background:" + it.col + ";border-radius:8px;color:#111;font-weight:bold;");
                    stLeft->setText("Minecraft " + it.ver);
                    buildGrid(it.name);
                });

                g->addWidget(tile, row, col++);
                if (col >= 5) { col = 0; row++; }
            }
            gridLay->addWidget(w);
        }
        gridLay->addStretch();
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    PrismWin w;
    w.show();
    return app.exec();
}
