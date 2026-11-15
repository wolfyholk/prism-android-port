#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QComboBox>
#include <QMap>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class VersionPickerDialog : public QDialog {
public:
    QString selectedVer = "1.21.4";
    QString selectedLoader = "Vanilla";

    VersionPickerDialog(QNetworkAccessManager *net, QWidget *parent) : QDialog(parent) {
        setWindowTitle("Add Instance");
        resize(460, 420);
        setStyleSheet("background:#1E1E1E;color:#FFF;");
        auto *root = new QVBoxLayout(this);

        // Верхняя кнопка со стрелкой (как на схеме: [v] 1.21.4)
        auto *btnDrop = new QPushButton("⌄   " + selectedVer);
        btnDrop->setStyleSheet("text-align:left;background:#2B2B2B;border:1px solid #444;padding:8px;font-weight:bold;");
        root->addWidget(btnDrop);

        // Две колонки: слева основные ветки (1.8 .. 26), справа подверсии (.1, .2, .3, .4...)
        auto *cols = new QHBoxLayout();
        auto *lwMajor = new QListWidget();
        auto *lwSub = new QListWidget();
        lwMajor->setFixedWidth(130);
        lwMajor->setStyleSheet("background:#252525;font-size:15px;");
        lwSub->setStyleSheet("background:#2A2A2A;font-size:15px;");
        cols->addWidget(lwMajor);
        cols->addWidget(lwSub, 1);
        root->addLayout(cols, 1);

        // Базовый словарь веток (сразу работает даже без интернета)
        QMap<QString, QStringList> tree = {
            {"1.8",  {"1.8", "1.8.8", "1.8.9"}},
            {"1.9",  {"1.9", "1.9.2", "1.9.4"}},
            {"1.10", {"1.10", "1.10.2"}},
            {"1.11", {"1.11", "1.11.2"}},
            {"1.12", {"1.12", "1.12.1", "1.12.2"}},
            {"1.13", {"1.13", "1.13.2"}},
            {"1.14", {"1.14", "1.14.4"}},
            {"1.15", {"1.15", "1.15.2"}},
            {"1.16", {"1.16", "1.16.1", "1.16.2", "1.16.3", "1.16.4", "1.16.5"}},
            {"1.17", {"1.17", "1.17.1"}},
            {"1.18", {"1.18", "1.18.1", "1.18.2"}},
            {"1.19", {"1.19", "1.19.1", "1.19.2", "1.19.3", "1.19.4"}},
            {"1.20", {"1.20", "1.20.1", "1.20.2", "1.20.3", "1.20.4", "1.20.5", "1.20.6"}},
            {"1.21", {"1.21", "1.21.1", "1.21.2", "1.21.3", "1.21.4", "1.21.5", "1.21.8", "1.21.11"}},
            {"26",   {"26.1", "26.2", "26.3"}}
        };
        QStringList order = {"1.8","1.9","1.10","1.11","1.12","1.13","1.14","1.15","1.16","1.17","1.18","1.19","1.20","1.21","26"};

        auto fillMajor = [&]() {
            lwMajor->clear();
            for (const auto &k : order) if (tree.contains(k)) lwMajor->addItem(k);
        };
        fillMajor();

        // Подгрузка актуальных версий с серверов Mojang и раскладка по веткам
        auto *rep = net->get(QNetworkRequest(QUrl("https://mojang.com")));
        connect(rep, &QNetworkReply::finished, [&, rep]() {
            if (rep->error() == QNetworkReply::NoError) {
                auto arr = QJsonDocument::fromJson(rep->readAll()).object()["versions"].toArray();
                for (int i = arr.size() - 1; i >= 0; --i) {
                    auto o = arr[i].toObject();
                    QString id = o["id"].toString();
                    if (o["type"].toString() != "release" && !id.startsWith("26.")) continue;
                    QStringList parts = id.split('.');
                    QString maj = parts.size() >= 2 ? (parts[0] == "26" ? "26" : parts[0] + "." + parts[1]) : id;
                    if (order.contains(maj) && !tree[maj].contains(id)) tree[maj].append(id);
                }
            }
            rep->deleteLater();
        });

        connect(lwMajor, &QListWidget::currentTextChanged, [&](const QString &maj) {
            lwSub->clear();
            if (tree.contains(maj)) lwSub->addItems(tree[maj]);
        });

        connect(lwSub, &QListWidget::currentTextChanged, [&](const QString &sub) {
            if (!sub.isEmpty()) {
                selectedVer = sub;
                btnDrop->setText("⌄   " + selectedVer);
            }
        });

        lwMajor->setCurrentRow(13); // По умолчанию открыта ветка 1.21

        // Выбор загрузчика и кнопка создания
        auto *bot = new QHBoxLayout();
        auto *cbLd = new QComboBox();
        cbLd->addItems({"Vanilla", "Fabric", "Forge", "NeoForge", "Quilt"});
        auto *bOk = new QPushButton("Создать сборку");
        bOk->setStyleSheet("background:#2B7CD3;color:#fff;font-weight:bold;padding:8px 16px;");
        connect(bOk, &QPushButton::clicked, [&]() {
            selectedLoader = cbLd->currentText();
            accept();
        });
        bot->addWidget(new QLabel("Loader:"));
        bot->addWidget(cbLd);
        bot->addStretch();
        bot->addWidget(bOk);
        root->addLayout(bot);
    }
};
