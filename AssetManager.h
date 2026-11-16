#pragma once
#include <QObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProgressDialog>
#include <functional>
#include <memory>

class AssetManager {
public:
    // Универсальный быстрый загрузчик списка недостающих файлов (в 8 потоков)
    static void downloadBatch(QNetworkAccessManager *net, const QString &title,
                              QList<QPair<QString, QString>> items, QWidget *ui,
                              std::function<void()> onDone) {
        auto *q = new QList<QPair<QString, QString>>();
        for (const auto &it : items) {
            if (!QFile::exists(it.second) || QFileInfo(it.second).size() == 0) {
                q->append(it);
            }
        }
        int total = q->size();
        if (total == 0) { delete q; onDone(); return; }

        auto *dlg = new QProgressDialog(title + " (0 / " + QString::number(total) + ")",
                                        "Отмена", 0, total, ui);
        dlg->setWindowModality(Qt::WindowModal);
        dlg->show();

        auto doneCount = std::make_shared<int>(0);
        auto active = std::make_shared<int>(0);
        auto finished = std::make_shared<bool>(false);
        auto pump = std::make_shared<std::function<void()>>();

        *pump = [net, q, total, title, dlg, doneCount, active, finished, onDone, pump]() {
            if (*finished) return;
            if (dlg->wasCanceled() || (q->isEmpty() && *active == 0)) {
                *finished = true;
                dlg->close(); dlg->deleteLater();
                delete q;
                onDone();
                return;
            }
            while (*active < 8 && !q->isEmpty() && !dlg->wasCanceled()) {
                auto item = q->takeFirst();
                (*active)++;
                QDir().mkpath(QFileInfo(item.second).absolutePath());
                auto *r = net->get(QNetworkRequest(QUrl(item.first)));
                QObject::connect(r, &QNetworkReply::finished, [r, item, title, total, dlg, doneCount, active, pump]() {
                    if (r->error() == QNetworkReply::NoError) {
                        QFile f(item.second);
                        if (f.open(QIODevice::WriteOnly)) { f.write(r->readAll()); f.close(); }
                    }
                    r->deleteLater();
                    (*active)--;
                    (*doneCount)++;
                    dlg->setValue(*doneCount);
                    dlg->setLabelText(title + " (" + QString::number(*doneCount) + " / " + QString::number(total) + ")");
                    (*pump)();
                });
            }
        };
        (*pump)();
    }

    // Сохранение метаданных версии (<ver>.json) и докачка ВСЕХ недостающих Assets
    static void downloadAssets(QNetworkAccessManager *net, const QByteArray &verJsonBytes,
                               const QString &dataDir, const QString &mcVer,
                               QWidget *ui, std::function<void()> onDone) {
        QString verDir = dataDir + "/versions/" + mcVer;
        QDir().mkpath(verDir);
        QFile metaFile(verDir + "/" + mcVer + ".json");
        if (metaFile.open(QIODevice::WriteOnly)) { metaFile.write(verJsonBytes); metaFile.close(); }

        auto idxObj = QJsonDocument::fromJson(verJsonBytes).object()["assetIndex"].toObject();
        QString idxId = idxObj["id"].toString();
        QString idxUrl = idxObj["url"].toString();
        if (idxUrl.isEmpty()) { onDone(); return; }

        QString idxDir = dataDir + "/assets/indexes";
        QDir().mkpath(idxDir);
        QString idxPath = idxDir + "/" + idxId + ".json";

        auto *rep = net->get(QNetworkRequest(QUrl(idxUrl)));
        QObject::connect(rep, &QNetworkReply::finished, [net, rep, idxPath, dataDir, ui, onDone]() {
            if (rep->error() != QNetworkReply::NoError) { rep->deleteLater(); onDone(); return; }
            QByteArray idxBytes = rep->readAll();
            rep->deleteLater();
            QFile f(idxPath);
            if (f.open(QIODevice::WriteOnly)) { f.write(idxBytes); f.close(); }

            auto objs = QJsonDocument::fromJson(idxBytes).object()["objects"].toObject();
            QList<QPair<QString, QString>> list;
            for (auto it = objs.begin(); it != objs.end(); ++it) {
                QString hash = it.value().toObject()["hash"].toString();
                if (hash.size() < 2) continue;
                QString sub = hash.left(2);
                list.append({"https://minecraft.net" + sub + "/" + hash,
                             dataDir + "/assets/objects/" + sub + "/" + hash});
            }
            downloadBatch(net, "Докачка Minecraft Assets", list, ui, onDone);
        });
    }
};
