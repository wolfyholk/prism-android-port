#pragma once
#include <QObject>
#include <QString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QMessageBox>
#include <functional>

class ManifestClientLoader {
public:
    static void fetchVersionAndClient(QNetworkAccessManager *net, const QString &mcVer,
                                      const QString &dataDir, QWidget *ui,
                                      std::function<void(const QByteArray &, const QString &)> onReady) {
        QString ver = mcVer.trimmed();
        QString verDir = dataDir + "/versions/" + ver;
        QDir().mkpath(verDir);
        QString jsonPath = verDir + "/" + ver + ".json";
        QString jarPath  = verDir + "/client.jar";

        if (QFile::exists(jsonPath) && QFile::exists(jarPath) && QFileInfo(jarPath).size() > 1000000) {
            QFile f(jsonPath);
            if (f.open(QIODevice::ReadOnly)) {
                QByteArray raw = f.readAll(); f.close();
                onReady(raw, jarPath);
                return;
            }
        }

        auto *dlg = new QProgressDialog("Поиск " + ver + " в манифесте...", "Отмена", 0, 0, ui);
        dlg->setWindowModality(Qt::WindowModal); dlg->show();

        QNetworkRequest mReq(QUrl("https://mojang.com"));
        mReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        auto *rep = net->get(mReq);

        QObject::connect(rep, &QNetworkReply::finished, [net, rep, dlg, ver, jsonPath, jarPath, ui, onReady]() {
            dlg->close(); dlg->deleteLater();
            if (rep->error() != QNetworkReply::NoError) {
                QMessageBox::warning(ui, "Ошибка манифеста", rep->errorString());
                rep->deleteLater(); return;
            }
            auto arr = QJsonDocument::fromJson(rep->readAll()).object()["versions"].toArray();
            rep->deleteLater();

            QString verUrl;
            for (const auto &item : arr) {
                if (item.toObject()["id"].toString().trimmed() == ver) {
                    verUrl = item.toObject()["url"].toString();
                    break;
                }
            }
            if (verUrl.isEmpty()) {
                QMessageBox::warning(ui, "Ошибка", "Версия " + ver + " не найдена.");
                return;
            }

            QNetworkRequest vReq((QUrl(verUrl)));
            vReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            auto *vRep = net->get(vReq);

            QObject::connect(vRep, &QNetworkReply::finished, [net, vRep, ver, jsonPath, jarPath, ui, onReady]() {
                if (vRep->error() != QNetworkReply::NoError) {
                    QMessageBox::warning(ui, "Ошибка JSON", vRep->errorString());
                    vRep->deleteLater(); return;
                }
                QByteArray verJson = vRep->readAll();
                vRep->deleteLater();

                QFile jf(jsonPath);
                if (jf.open(QIODevice::WriteOnly)) { jf.write(verJson); jf.close(); }

                QString clientUrl = QJsonDocument::fromJson(verJson).object()
                                        ["downloads"].toObject()["client"].toObject()["url"].toString();
                if (clientUrl.isEmpty()) {
                    QMessageBox::warning(ui, "Ошибка", "Нет ссылки на client.jar");
                    return;
                }
                downloadJar(net, clientUrl, ver, jarPath, verJson, ui, onReady);
            });
        });
    }

private:
    static void downloadJar(QNetworkAccessManager *net, const QString &url, const QString &ver,
                            const QString &jarPath, const QByteArray &verJson, QWidget *ui,
                            std::function<void(const QByteArray &, const QString &)> onReady) {
        if (QFile::exists(jarPath) && QFileInfo(jarPath).size() > 1000000) {
            onReady(verJson, jarPath); return;
        }
        auto *dlg = new QProgressDialog("Установка client.jar (" + ver + ")...", "Отмена", 0, 100, ui);
        dlg->setWindowModality(Qt::WindowModal); dlg->show();

        QNetworkRequest req((QUrl(url)));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        auto *rep = net->get(req);

        QObject::connect(rep, &QNetworkReply::downloadProgress, [dlg](qint64 r, qint64 t) {
            if (t > 0) dlg->setValue(int((r * 100) / t));
        });
        QObject::connect(rep, &QNetworkReply::finished, [rep, dlg, jarPath, verJson, ui, onReady]() {
            dlg->close(); dlg->deleteLater();
            if (rep->error() != QNetworkReply::NoError) {
                QMessageBox::warning(ui, "Ошибка client.jar", rep->errorString());
                rep->deleteLater(); return;
            }
            QByteArray bytes = rep->readAll();
            rep->deleteLater();

            QDir().mkpath(QFileInfo(jarPath).absolutePath());
            QFile f(jarPath);
            if (!f.open(QIODevice::WriteOnly)) {
                QMessageBox::warning(ui, "Ошибка записи", "Нет прав на запись " + jarPath);
                return;
            }
            f.write(bytes); f.close();
            onReady(verJson, jarPath);
        });
    }
};
