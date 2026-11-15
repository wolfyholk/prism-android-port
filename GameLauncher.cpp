#include "GameLauncher.h"
#include "TouchControls.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
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
    QDir().mkpath(cfg.dataDir + "/libraries");
    QDir().mkpath(verDir);

    // Всегда проверяем манифест версии, чтобы докачать все библиотеки libraries/*.jar
    downloadVersionManifest(cfg, parentUi);
}

void GameLauncher::downloadVersionManifest(const LaunchConfig &cfg, QWidget *parentUi) {
    auto *dlg = new QProgressDialog("Проверка манифеста " + cfg.mcVer + "...", "Отмена", 0, 0, parentUi);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->show();

    QNetworkRequest req(QUrl("https://mojang.com"));
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
            QMessageBox::warning(parentUi, "Ошибка", "Версия " + cfg.mcVer + " не найдена.");
            return;
        }

        auto *vRep = net->get(QNetworkRequest(QUrl(verUrl)));
        connect(vRep, &QNetworkReply::finished, [this, vRep, cfg, parentUi]() {
            if (vRep->error() != QNetworkReply::NoError) {
                QMessageBox::warning(parentUi, "Ошибка", vRep->errorString());
                vRep->deleteLater();
                return;
            }
            QByteArray rawJson = vRep->readAll();
            vRep->deleteLater();
            parseAndDownloadLibraries(rawJson, cfg, parentUi);
        });
    });
}

void GameLauncher::parseAndDownloadLibraries(const QByteArray &verJsonBytes,
                                             const LaunchConfig &cfg, QWidget *parentUi) {
    auto doc = QJsonDocument::fromJson(verJsonBytes).object();
    QString jarUrl = doc["downloads"].toObject()["client"].toObject()["url"].toString();

    libQueue.clear();
    auto libs = doc["libraries"].toArray();
    for (const auto &item : libs) {
        auto libObj = item.toObject();
        // Пропускаем нативные библиотеки для Windows/macOS
        if (libObj.contains("rules")) {
            bool allowLinux = false;
            for (const auto &r : libObj["rules"].toArray()) {
                auto ro = r.toObject();
                QString osName = ro["os"].toObject()["name"].toString();
                if (ro["action"].toString() == "allow" && (osName.isEmpty() || osName == "linux")) {
                    allowLinux = true;
                }
            }
            if (!allowLinux) continue;
        }

        auto art = libObj["downloads"].toObject()["artifact"].toObject();
        QString url = art["url"].toString();
        QString path = art["path"].toString();
        if (!url.isEmpty() && !path.isEmpty()) {
            QString localPath = cfg.dataDir + "/libraries/" + path;
            if (!QFile::exists(localPath) || QFileInfo(localPath).size() == 0) {
                libQueue.append(qMakePair(url, localPath));
            }
        }
    }

    totalLibs = libQueue.size();
    if (totalLibs == 0) {
        // Все библиотеки уже скачаны — проверяем client.jar
        QString jarPath = cfg.dataDir + "/versions/" + cfg.mcVer + "/client.jar";
        if (QFile::exists(jarPath) && QFileInfo(jarPath).size() > 1000000) {
            startJavaProcess(jarPath, cfg, parentUi);
        } else {
            downloadClientJar(jarUrl, jarPath, cfg, parentUi);
        }
        return;
    }

    auto *dlg = new QProgressDialog("Загрузка библиотек (0 / " + QString::number(totalLibs) + ")...",
                                    "Отмена", 0, totalLibs, parentUi);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->show();
    downloadNextLibrary(jarUrl, cfg, parentUi, dlg);
}

void GameLauncher::downloadNextLibrary(const QString &jarUrl, const LaunchConfig &cfg,
                                       QWidget *parentUi, QProgressDialog *dlg) {
    if (dlg->wasCanceled() || libQueue.isEmpty()) {
        dlg->close();
        dlg->deleteLater();
        QString jarPath = cfg.dataDir + "/versions/" + cfg.mcVer + "/client.jar";
        if (QFile::exists(jarPath) && QFileInfo(jarPath).size() > 1000000) {
            startJavaProcess(jarPath, cfg, parentUi);
        } else {
            downloadClientJar(jarUrl, jarPath, cfg, parentUi);
        }
        return;
    }

    auto next = libQueue.takeFirst();
    int done = totalLibs - libQueue.size();
    dlg->setValue(done);
    dlg->setLabelText("Загрузка библиотек (" + QString::number(done) + " / " +
                      QString::number(totalLibs) + "):\n" + QFileInfo(next.second).fileName());

    QDir().mkpath(QFileInfo(next.second).absolutePath());
    auto *rep = net->get(QNetworkRequest(QUrl(next.first)));
    connect(rep, &QNetworkReply::finished, [this, rep, next, jarUrl, cfg, parentUi, dlg]() {
        if (rep->error() == QNetworkReply::NoError) {
            QFile f(next.second);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(rep->readAll());
                f.close();
            }
        }
        rep->deleteLater();
        downloadNextLibrary(jarUrl, cfg, parentUi, dlg);
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
    QMessageBox::information(parentUi, "Библиотеки и клиент готовы!",
        "Аккаунт: " + cfg.nick + " (" + cfg.accType + ")\n" +
        "Версия: " + cfg.mcVer + " (" + cfg.loader + ")\n" +
        "Библиотеки: скачаны в libraries/\n" +
        "Клиент: " + QString::number(mb, 'f', 1) + " МБ\n" +
        "Нужная Java: " + selectJavaVersion(cfg.mcVer));

    TouchOverlay overlay(cfg.mcVer, parentUi);
    overlay.exec();
}
