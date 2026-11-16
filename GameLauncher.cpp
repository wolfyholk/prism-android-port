#include "GameLauncher.h"
#include "AssetManager.h"
#include "JvmRunner.h"
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

GameLauncher::GameLauncher(QNetworkAccessManager *nam, QObject *parent) : QObject(parent), net(nam) {}

QString GameLauncher::selectJavaVersion(const QString &v) {
    return (v.startsWith("26.") || v.startsWith("1.21")) ? "21" : "17";
}

void GameLauncher::prepareAndLaunch(const LaunchConfig &cfg, QWidget *ui) {
    QString vDir = cfg.dataDir + "/versions/" + cfg.mcVer.trimmed();
    QDir().mkpath(cfg.dataDir + "/instances/" + cfg.instId + "/mods");
    QDir().mkpath(cfg.dataDir + "/libraries");
    QDir().mkpath(cfg.dataDir + "/assets");
    QDir().mkpath(cfg.dataDir + "/natives");
    QDir().mkpath(vDir);
    downloadVersionManifest(cfg, ui);
}

void GameLauncher::downloadVersionManifest(const LaunchConfig &cfg, QWidget *ui) {
    auto *dlg = new QProgressDialog("Метаданные " + cfg.mcVer + "...", "Отмена", 0, 0, ui);
    dlg->setWindowModality(Qt::WindowModal); dlg->show();
    auto *rep = net->get(QNetworkRequest(QUrl("https://mojang.com")));
    connect(rep, &QNetworkReply::finished, [this, rep, dlg, cfg, ui]() {
        dlg->close(); dlg->deleteLater();
        if (rep->error() != QNetworkReply::NoError) { QMessageBox::warning(ui, "Ошибка", rep->errorString()); rep->deleteLater(); return; }
        auto arr = QJsonDocument::fromJson(rep->readAll()).object()["versions"].toArray();
        rep->deleteLater();
        QString url, tv = cfg.mcVer.trimmed();
        for (const auto &v : arr) if (v.toObject()["id"].toString().trimmed() == tv) { url = v.toObject()["url"].toString(); break; }
        if (url.isEmpty()) { QMessageBox::warning(ui, "Ошибка", "Версия " + tv + " не найдена."); return; }
        auto *vr = net->get(QNetworkRequest(QUrl(url)));
        connect(vr, &QNetworkReply::finished, [this, vr, cfg, ui]() {
            if (vr->error() == QNetworkReply::NoError) {
                QByteArray raw = vr->readAll(); vr->deleteLater();
                AssetManager::downloadAssets(net, raw, cfg.dataDir, cfg.mcVer.trimmed(), ui, [this, raw, cfg, ui]() {
                    parseAndDownloadLibraries(raw, cfg, ui);
                });
            } else vr->deleteLater();
        });
    });
}

void GameLauncher::parseAndDownloadLibraries(const QByteArray &raw, const LaunchConfig &cfg, QWidget *ui) {
    auto doc = QJsonDocument::fromJson(raw).object();
    QString jarUrl = doc["downloads"].toObject()["client"].toObject()["url"].toString();
    libQueue.clear();
    for (const auto &item : doc["libraries"].toArray()) {
        auto lo = item.toObject();
        if (lo.contains("rules")) {
            bool ok = false;
            for (const auto &r : lo["rules"].toArray()) {
                QString os = r.toObject()["os"].toObject()["name"].toString();
                if (r.toObject()["action"] == "allow" && (os.isEmpty() || os == "linux")) ok = true;
            }
            if (!ok) continue;
        }
        auto art = lo["downloads"].toObject()["artifact"].toObject();
        QString u = art["url"].toString(), p = art["path"].toString();
        if (!u.isEmpty() && !p.isEmpty()) {
            QString lp = cfg.dataDir + "/libraries/" + p;
            if (!QFile::exists(lp) || QFileInfo(lp).size() == 0) libQueue.append({u, lp});
        }
    }
    totalLibs = libQueue.size();
    QString jp = cfg.dataDir + "/versions/" + cfg.mcVer.trimmed() + "/client.jar";
    if (!totalLibs) {
        if (QFile::exists(jp) && QFileInfo(jp).size() > 1000000) JvmRunner::run(net, jp, cfg, ui);
        else downloadClientJar(jarUrl, jp, cfg, ui);
        return;
    }
    auto *dlg = new QProgressDialog("Загрузка Minecraft Libraries...", "Отмена", 0, totalLibs, ui);
    dlg->setWindowModality(Qt::WindowModal); dlg->show();
    downloadNextLibrary(jarUrl, cfg, ui, dlg);
}

void GameLauncher::downloadNextLibrary(const QString &jarUrl, const LaunchConfig &cfg, QWidget *ui, QProgressDialog *dlg) {
    if (dlg->wasCanceled() || libQueue.isEmpty()) {
        dlg->close(); dlg->deleteLater();
        QString jp = cfg.dataDir + "/versions/" + cfg.mcVer.trimmed() + "/client.jar";
        if (QFile::exists(jp) && QFileInfo(jp).size() > 1000000) JvmRunner::run(net, jp, cfg, ui);
        else downloadClientJar(jarUrl, jp, cfg, ui);
        return;
    }
    auto nxt = libQueue.takeFirst();
    dlg->setValue(totalLibs - libQueue.size());
    QDir().mkpath(QFileInfo(nxt.second).absolutePath());
    auto *rep = net->get(QNetworkRequest(QUrl(nxt.first)));
    connect(rep, &QNetworkReply::finished, [this, rep, nxt, jarUrl, cfg, ui, dlg]() {
        if (rep->error() == QNetworkReply::NoError) {
            QFile f(nxt.second); if (f.open(QIODevice::WriteOnly)) { f.write(rep->readAll()); f.close(); }
        }
        rep->deleteLater();
        downloadNextLibrary(jarUrl, cfg, ui, dlg);
    });
}

void GameLauncher::downloadClientJar(const QString &url, const QString &jp, const LaunchConfig &cfg, QWidget *ui) {
    auto *dlg = new QProgressDialog("Скачивание client.jar...", "Отмена", 0, 100, ui);
    dlg->setWindowModality(Qt::WindowModal); dlg->show();
    auto *rep = net->get(QNetworkRequest(QUrl(url)));
    connect(rep, &QNetworkReply::downloadProgress, [dlg](qint64 r, qint64 t) { if (t > 0) dlg->setValue(int(r * 100 / t)); });
    connect(rep, &QNetworkReply::finished, [this, rep, dlg, jp, cfg, ui]() {
        dlg->close(); dlg->deleteLater();
        if (rep->error() == QNetworkReply::NoError) {
            QFile f(jp); if (f.open(QIODevice::WriteOnly)) { f.write(rep->readAll()); f.close(); }
            rep->deleteLater(); JvmRunner::run(net, jp, cfg, ui);
        } else rep->deleteLater();
    });
}
