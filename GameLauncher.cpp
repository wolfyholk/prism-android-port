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
    QString v = cfg.mcVer.trimmed();
    QDir().mkpath(cfg.dataDir + "/instances/" + cfg.instId + "/mods");
    QDir().mkpath(cfg.dataDir + "/libraries");
    QDir().mkpath(cfg.dataDir + "/assets");
    QDir().mkpath(cfg.dataDir + "/natives");
    QDir().mkpath(cfg.dataDir + "/versions/" + v);

    QString localJson = cfg.dataDir + "/versions/" + v + "/" + v + ".json";
    if (QFile::exists(localJson) && QFileInfo(localJson).size() > 1000) {
        QFile f(localJson);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray raw = f.readAll(); f.close();
            parseAndDownloadLibraries(raw, cfg, ui);
            return;
        }
    }
    downloadVersionManifest(cfg, ui);
}

void GameLauncher::downloadVersionManifest(const LaunchConfig &cfg, QWidget *ui) {
    auto *dlg = new QProgressDialog("Получение списка версий...", "Отмена", 0, 0, ui);
    dlg->setWindowModality(Qt::WindowModal); dlg->show();

    auto *rep = net->get(QNetworkRequest(QUrl("https://mojang.com")));
    connect(rep, &QNetworkReply::finished, [this, rep, dlg, cfg, ui]() {
        dlg->close(); dlg->deleteLater();
        QString tv = cfg.mcVer.trimmed();
        QString verUrl;

        if (rep->error() == QNetworkReply::NoError) {
            auto arr = QJsonDocument::fromJson(rep->readAll()).object()["versions"].toArray();
            for (const auto &v : arr) {
                QString id = v.toObject()["id"].toString().trimmed();
                if (id == tv) { verUrl = v.toObject()["url"].toString(); break; }
            }
            if (verUrl.isEmpty()) {
                for (const auto &v : arr) {
                    if (v.toObject()["type"].toString() == "release") {
                        verUrl = v.toObject()["url"].toString();
                        break;
                    }
                }
            }
        }
        rep->deleteLater();

        if (verUrl.isEmpty()) {
            QMessageBox::warning(ui, "Ошибка сети", "Не удалось получить манифест версий Mojang. Проверьте интернет.");
            return;
        }

        auto *vr = net->get(QNetworkRequest(QUrl(verUrl)));
        connect(vr, &QNetworkReply::finished, [this, vr, cfg, ui]() {
            if (vr->error() == QNetworkReply::NoError) {
                QByteArray raw = vr->readAll(); vr->deleteLater();
                AssetManager::downloadAssets(net, raw, cfg.dataDir, cfg.mcVer.trimmed(), ui, [this, raw, cfg, ui]() {
                    parseAndDownloadLibraries(raw, cfg, ui);
                });
            } else {
                QMessageBox::warning(ui, "Ошибка метаданных", vr->errorString());
                vr->deleteLater();
            }
        });
    });
}

void GameLauncher::parseAndDownloadLibraries(const QByteArray &raw, const LaunchConfig &cfg, QWidget *ui) {
    auto doc = QJsonDocument::fromJson(raw).object();
    QString jarUrl = doc["downloads"].toObject()["client"].toObject()["url"].toString();
    QList<QPair<QString, QString>> libs;

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
        if (!art["url"].toString().isEmpty() && !art["path"].toString().isEmpty()) {
            libs.append({art["url"].toString(), cfg.dataDir + "/libraries/" + art["path"].toString()});
        }
    }

    AssetManager::downloadBatch(net, "Загрузка библиотек", libs, ui, [this, jarUrl, cfg, ui]() {
        QString jp = cfg.dataDir + "/versions/" + cfg.mcVer.trimmed() + "/client.jar";
        if (QFile::exists(jp) && QFileInfo(jp).size() > 1000000) {
            JvmRunner::run(net, jp, cfg, ui);
        } else {
            downloadClientJar(jarUrl, jp, cfg, ui);
        }
    });
}

void GameLauncher::downloadNextLibrary(const QString &, const LaunchConfig &, QWidget *, QProgressDialog *) {}

void GameLauncher::downloadClientJar(const QString &url, const QString &jp, const LaunchConfig &cfg, QWidget *ui) {
    QString targetUrl = url.isEmpty()
        ? "https://mojang.com"
        : url;

    auto *dlg = new QProgressDialog("Скачивание client.jar (" + cfg.mcVer + ")...", "Отмена", 0, 100, ui);
    dlg->setWindowModality(Qt::WindowModal); dlg->show();

    QNetworkRequest req((QUrl(targetUrl)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *rep = net->get(req);
    connect(rep, &QNetworkReply::downloadProgress, [dlg](qint64 r, qint64 t) {
        if (t > 0) dlg->setValue(int(r * 100 / t));
    });
    connect(rep, &QNetworkReply::finished, [this, rep, dlg, jp, cfg, ui]() {
        dlg->close(); dlg->deleteLater();
        if (rep->error() == QNetworkReply::NoError) {
            QByteArray data = rep->readAll();
            rep->deleteLater();
            if (data.size() < 1000000) {
                QMessageBox::warning(ui, "Ошибка client.jar", "Файл client.jar повреждён (размер < 1 МБ).");
                return;
            }
            QDir().mkpath(QFileInfo(jp).absolutePath());
            QFile f(jp);
            if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
            JvmRunner::run(net, jp, cfg, ui);
        } else {
            QMessageBox::warning(ui, "Ошибка скачивания client.jar", rep->errorString());
            rep->deleteLater();
        }
    });
}
