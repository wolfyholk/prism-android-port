#include "GameLauncher.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QProgressDialog>
#include <QMessageBox>

GameLauncher::GameLauncher(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), net(nam) {}

QString GameLauncher::selectJavaVersion(const QString &mcVer) {
    if (mcVer.startsWith("26.")) return "Java 25+";
    if (mcVer.startsWith("1.21") || mcVer == "1.20.5" || mcVer == "1.20.6") return "Java 21+";
    if (mcVer.startsWith("1.17") || mcVer.startsWith("1.18") ||
        mcVer.startsWith("1.19") || mcVer.startsWith("1.20")) return "Java 17+";
    return "Java 8+";
}

void GameLauncher::prepareAndLaunch(const LaunchConfig &cfg, QWidget *parentUi) {
    QString verDir = cfg.dataDir + "/versions/" + cfg.mcVer;
    QDir().mkpath(cfg.dataDir + "/instances/" + cfg.instId + "/mods");
    QDir().mkpath(verDir);

    QString jarPath = verDir + "/client.jar";
    if (QFile::exists(jarPath) && QFileInfo(jarPath).size() > 1000000) {
        startJavaProcess(jarPath, cfg, parentUi);
    } else {
        downloadVersionManifest(cfg, parentUi);
    }
}

void GameLauncher::downloadVersionManifest(const LaunchConfig &cfg, QWidget *parentUi) {
    auto *dlg = new QProgressDialog("Поиск версии " + cfg.mcVer + "...", "Отмена", 0, 0, parentUi);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->show();

    QNetworkRequest req(QUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"));
    auto *rep = net->get(req);

    connect(rep, &QNetworkReply::finished, [this, rep, dlg, cfg, parentUi]() {
        dlg->close(); dlg->deleteLater();
        if (rep->error() != QNetworkReply::NoError) {
            QMessageBox::warning(parentUi, "Ошибка сети", rep->errorString());
            rep->deleteLater();
            return;
        }
        auto arr = QJsonDocument::fromJson(rep->readAll()).object()["versions"].toArray();
        rep->deleteLater();

        QString verUrl;
        for (const auto &v : arr) {
            if (v.toObject()["id"].toString() == cfg.mcVer) {
                verUrl = v.toObject()["url"].toString();
                break;
            }
        }
        if (verUrl.isEmpty()) {
            QMessageBox::warning(parentUi, "Ошибка", "Версия " + cfg.mcVer + " не найдена в манифесте.");
            return;
        }

        auto *vRep = net->get(QNetworkRequest(QUrl(verUrl)));
        connect(vRep, &QNetworkReply::finished, [this, vRep, cfg, parentUi]() {
            if (vRep->error() != QNetworkReply::NoError) {
                QMessageBox::warning(parentUi, "Ошибка", vRep->errorString());
                vRep->deleteLater();
                return;
            }
            auto doc = QJsonDocument::fromJson(vRep->readAll()).object();
            vRep->deleteLater();
            QString jarUrl = doc["downloads"].toObject()["client"].toObject()["url"].toString();
            downloadClientJar(jarUrl, cfg.dataDir + "/versions/" + cfg.mcVer + "/client.jar", cfg, parentUi);
        });
    });
}

void GameLauncher::downloadClientJar(const QString &url, const QString &jarPath,
                                     const LaunchConfig &cfg, QWidget *parentUi) {
    auto *dlg = new QProgressDialog("Скачивание client.jar (" + cfg.mcVer + ")...", "Отмена", 0, 100, parentUi);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->show();

    auto *rep = net->get(QNetworkRequest(QUrl(url)));
    connect(rep, &QNetworkReply::downloadProgress, [dlg](qint64 r, qint64 t) {
        if (t > 0) dlg->setValue(int((r * 100) / t));
    });

    connect(rep, &QNetworkReply::finished, [this, rep, dlg, jarPath, cfg, parentUi]() {
        dlg->close(); dlg->deleteLater();
        if (rep->error() != QNetworkReply::NoError) {
            QMessageBox::warning(parentUi, "Ошибка загрузки", rep->errorString());
            rep->deleteLater();
            return;
        }
        QFile f(jarPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(rep->readAll());
            f.close();
        }
        rep->deleteLater();
        startJavaProcess(jarPath, cfg, parentUi);
    });
}

void GameLauncher::startJavaProcess(const QString &jarPath, const LaunchConfig &cfg,
                                    QWidget *parentUi) {
    double mb = QFileInfo(jarPath).size() / (1024.0 * 1024.0);
    QMessageBox::information(parentUi, "client.jar установлен!",
        "Версия: " + cfg.mcVer + " (" + cfg.loader + ")\n" +
        "Файл: " + jarPath + "\n" +
        "Размер: " + QString::number(mb, 'f', 1) + " МБ\n" +
        "Нужная Java: " + selectJavaVersion(cfg.mcVer));
}
