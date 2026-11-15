#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QInputDialog>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QTimer>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>

struct AccountInfo {
    QString name;
    QString type; // "Offline" или "MSA"
    QString uuid;
    QString token;
};

class AccountManager {
public:
    static QList<AccountInfo> load(const QString &baseDir) {
        QList<AccountInfo> list;
        QFile f(baseDir + "/accounts.json");
        if (f.open(QIODevice::ReadOnly)) {
            auto arr = QJsonDocument::fromJson(f.readAll()).object()["accounts"].toArray();
            for (const auto &v : arr) {
                auto o = v.toObject();
                list.append({o["name"].toString(), o["type"].toString(),
                             o["uuid"].toString(), o["token"].toString()});
            }
            f.close();
        }
        return list;
    }

    static void save(const QString &baseDir, const QList<AccountInfo> &list) {
        QJsonArray arr;
        for (const auto &a : list) {
            QJsonObject o;
            o["name"] = a.name; o["type"] = a.type;
            o["uuid"] = a.uuid; o["token"] = a.token;
            arr.append(o);
        }
        QJsonObject root;
        root["formatVersion"] = 3;
        root["accounts"] = arr;
        QFile f(baseDir + "/accounts.json");
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QJsonDocument(root).toJson());
            f.close();
        }
    }

    static QString makeOfflineUuid(const QString &nick) {
        QByteArray hash = QCryptographicHash::hash(
            ("OfflinePlayer:" + nick).toUtf8(), QCryptographicHash::Md5);
        hash[6] = (hash[6] & 0x0f) | 0x30;
        hash[8] = (hash[8] & 0x3f) | 0x80;
        return QString(hash.toHex());
    }

    static AccountInfo openDialog(const QString &baseDir, QNetworkAccessManager *net, QWidget *parent) {
        QList<AccountInfo> list = load(baseDir);
        QDialog dlg(parent);
        dlg.setWindowTitle("Accounts - Prism Launcher");
        dlg.resize(460, 320);
        auto *lay = new QVBoxLayout(&dlg);

        auto *lw = new QListWidget();
        auto refresh = [&]() {
            lw->clear();
            for (const auto &a : list) {
                lw->addItem(a.name + "  [" + (a.type == "MSA" ? "Microsoft License" : "Offline") + "]");
            }
            if (!list.isEmpty()) lw->setCurrentRow(0);
        };
        refresh();
        lay->addWidget(lw);

        auto *hb = new QHBoxLayout();
        auto *bOff = new QPushButton("+ Add Offline");
        auto *bMsa = new QPushButton("+ Add Microsoft");
        auto *bDel = new QPushButton("Remove");
        auto *bOk  = new QPushButton("Select");
        bOk->setStyleSheet("background:#2B7CD3;color:#fff;font-weight:bold;padding:6px 14px;");

        QObject::connect(bOff, &QPushButton::clicked, [&]() {
            bool ok;
            QString nick = QInputDialog::getText(&dlg, "Offline Account", "Введите никнейм:",
                                                 QLineEdit::Normal, "Player", &ok);
            if (ok && !nick.trimmed().isEmpty()) {
                QString n = nick.trimmed();
                list.prepend({n, "Offline", makeOfflineUuid(n), "0"});
                save(baseDir, list);
                refresh();
            }
        });

        QObject::connect(bMsa, &QPushButton::clicked, [&]() {
            // Официальный Microsoft Device Code Flow (используется в Prism Launcher)
            QNetworkRequest req(QUrl("https://microsoftonline.com"));
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            QUrlQuery q;
            q.addQueryItem("client_id", "00000000402b5328");
            q.addQueryItem("scope", "XboxLive.signin offline_access");

            auto *rep = net->post(req, q.toString(QUrl::FullyEncoded).toUtf8());
            QObject::connect(rep, &QNetworkReply::finished, [&, rep]() {
                if (rep->error() != QNetworkReply::NoError) {
                    QMessageBox::warning(&dlg, "Microsoft Login", "Ошибка сети: " + rep->errorString());
                    rep->deleteLater();
                    return;
                }
                auto obj = QJsonDocument::fromJson(rep->readAll()).object();
                rep->deleteLater();
                QString userCode = obj["user_code"].toString();
                QString devCode  = obj["device_code"].toString();
                QString verUri   = obj["verification_uri"].toString();

                QGuiApplication::clipboard()->setText(userCode);
                QDesktopServices::openUrl(QUrl(verUri));

                auto ans = QMessageBox::information(&dlg, "Вход в Microsoft",
                    "Код скопирован в буфер обмена: " + userCode + "\n\n"
                    "1. Вставьте код на открывшейся странице " + verUri + "\n"
                    "2. Войдите в свой аккаунт Microsoft с лицензией Minecraft.\n"
                    "3. После входа нажмите ОК здесь.",
                    QMessageBox::Ok | QMessageBox::Cancel);

                if (ans == QMessageBox::Ok) {
                    QNetworkRequest tReq(QUrl("https://microsoftonline.com"));
                    tReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
                    QUrlQuery tq;
                    tq.addQueryItem("grant_type", "urn:ietf:params:oauth:grant-type:device_code");
                    tq.addQueryItem("client_id", "00000000402b5328");
                    tq.addQueryItem("device_code", devCode);

                    auto *tRep = net->post(tReq, tq.toString(QUrl::FullyEncoded).toUtf8());
                    QObject::connect(tRep, &QNetworkReply::finished, [&, tRep]() {
                        auto tObj = QJsonDocument::fromJson(tRep->readAll()).object();
                        tRep->deleteLater();
                        if (tObj.contains("access_token")) {
                            QString msToken = tObj["access_token"].toString();
                            QString nick = QInputDialog::getText(&dlg, "Профиль",
                                "Вход выполнен! Введите ваш игровой никнейм лицензии:",
                                QLineEdit::Normal, "Steve");
                            if (!nick.trimmed().isEmpty()) {
                                list.prepend({nick.trimmed(), "MSA", makeOfflineUuid(nick.trimmed()), msToken});
                                save(baseDir, list);
                                refresh();
                            }
                        } else {
                            QMessageBox::warning(&dlg, "Ошибка входа",
                                "Авторизация не завершена: " + tObj["error_description"].toString());
                        }
                    });
                }
            });
        });

        QObject::connect(bDel, &QPushButton::clicked, [&]() {
            int r = lw->currentRow();
            if (r >= 0) {
                list.removeAt(r);
                save(baseDir, list);
                refresh();
            }
        });

        QObject::connect(bOk, &QPushButton::clicked, &dlg, &QDialog::accept);

        hb->addWidget(bOff); hb->addWidget(bMsa); hb->addWidget(bDel); hb->addStretch(); hb->addWidget(bOk);
        lay->addLayout(hb);

        dlg.exec();
        int idx = lw->currentRow();
        if (idx >= 0 && idx < list.size()) return list[idx];
        if (!list.isEmpty()) return list.first();
        return {"Player", "Offline", makeOfflineUuid("Player"), "0"};
    }
};
