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
#include <QProcess>

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
    QStringList cpList;
    QDir libDir(cfg.dataDir + "/libraries");
    QStringList filters = {"*.jar"};
    QList<QDir> stack = {libDir};
    while (!stack.isEmpty()) {
        QDir cur = stack.takeFirst();
        for (const auto &fi : cur.entryInfoList(filters, QDir::Files)) {
            cpList.append(fi.absoluteFilePath());
        }
        for (const auto &sub : cur.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            stack.append(QDir(sub.absoluteFilePath()));
        }
    }
    cpList.append(jarPath);
    QString fullClasspath = cpList.join(":");

    QString skinPackSrc = cfg.dataDir + "/skins/CustomSkinPack";
    QString instGameDir = cfg.dataDir + "/instances/" + cfg.instId + "/.minecraft";
    QDir().mkpath(instGameDir + "/resourcepacks");
    if (QDir(skinPackSrc).exists()) {
        QString dstSkin = instGameDir + "/resourcepacks/CustomSkinPack/assets/minecraft/textures/entity/player/wide";
        QDir().mkpath(dstSkin);
        QFile::copy(cfg.dataDir + "/skins/" + cfg.nick + ".png", dstSkin + "/steve.png");
    }

    QString javaExec = cfg.dataDir + "/java/jre21/bin/java";
    if (!QFile::exists(javaExec)) javaExec = "/system/bin/app_process64";

    QStringList args;
    args << "-Xmx" + QString::number(cfg.ramMb) + "M"
         << "-Xms512M"
         << "-Dorg.lwjgl.opengl.libname=libgl4es_114.so"
         << "-Dglfwstub.windowWidth=1280"
         << "-Dglfwstub.windowHeight=720"
         << "-Duser.home=" + cfg.dataDir
         << "-cp" << fullClasspath
         << "net.minecraft.client.main.Main"
         << "--username" << cfg.nick
         << "--version" << cfg.mcVer
         << "--gameDir" << instGameDir
         << "--assetsDir" << cfg.dataDir + "/assets"
         << "--uuid" << cfg.uuid
         << "--accessToken" << cfg.token
         << "--userType" << (cfg.accType == "MSA" ? "msa" : "legacy");

    auto *jvmProcess = new QProcess(parentUi);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LIBGL_ES", "2");
    env.insert("LIBGL_MIPMAP", "3");
    env.insert("LIBGL_NORMALIZE", "1");
    jvmProcess->setProcessEnvironment(env);
    jvmProcess->setWorkingDirectory(instGameDir);
    jvmProcess->start(javaExec, args);

    TouchOverlay overlay(cfg.mcVer + " | " + cfg.nick, parentUi);
    overlay.exec();

    if (jvmProcess->state() != QProcess::NotRunning) {
        jvmProcess->kill();
    }
}
