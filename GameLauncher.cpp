#include "GameLauncher.h"
#include "AssetManager.h"
#include "JvmRunner.h"
#include "manifest minecraft/ManifestClientLoader.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

GameLauncher::GameLauncher(QNetworkAccessManager *nam, QObject *parent) : QObject(parent), net(nam) {}

QString GameLauncher::selectJavaVersion(const QString &v) {
    return (v.startsWith("26.") || v.startsWith("1.21")) ? "21" : "17";
}

void GameLauncher::prepareAndLaunch(const LaunchConfig &cfg, QWidget *ui) {
    QDir().mkpath(cfg.dataDir + "/instances/" + cfg.instId + "/mods");
    QDir().mkpath(cfg.dataDir + "/libraries");
    QDir().mkpath(cfg.dataDir + "/assets");
    QDir().mkpath(cfg.dataDir + "/natives");

    ManifestClientLoader::fetchVersionAndClient(net, cfg.mcVer, cfg.dataDir, ui,
        [this, cfg, ui](const QByteArray &verJson, const QString &jarPath) {
            AssetManager::downloadAssets(net, verJson, cfg.dataDir, cfg.mcVer.trimmed(), ui,
                [this, verJson, jarPath, cfg, ui]() {
                    auto doc = QJsonDocument::fromJson(verJson).object();
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

                    AssetManager::downloadBatch(net, "Загрузка библиотек", libs, ui, [this, jarPath, cfg, ui]() {
                        JvmRunner::run(net, jarPath, cfg, ui);
                    });
                });
        });
}

void GameLauncher::downloadVersionManifest(const LaunchConfig &, QWidget *) {}
void GameLauncher::parseAndDownloadLibraries(const QByteArray &, const LaunchConfig &, QWidget *) {}
void GameLauncher::downloadNextLibrary(const QString &, const LaunchConfig &, QWidget *, QProgressDialog *) {}
void GameLauncher::downloadClientJar(const QString &, const QString &, const LaunchConfig &, QWidget *) {}
