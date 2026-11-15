#include "GameLauncher.h"
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
    // Автоматический выбор нужной версии Java как в оригинальном Prism Launcher
    if (mcVer.startsWith("1.12") || mcVer.startsWith("1.8")) return "Java 8";
    if (mcVer.startsWith("1.16")) return "Java 11";
    if (mcVer.startsWith("1.17") || mcVer.startsWith("1.18") ||
        mcVer.startsWith("1.19") || mcVer.startsWith("1.20.1") ||
        mcVer.startsWith("1.20.4")) return "Java 17";
    return "Java 21"; // Для 1.20.5+, 1.21.x и 26.x
}

void GameLauncher::prepareAndLaunch(const LaunchConfig &cfg, QWidget *parentUi) {
    QString instDir = cfg.dataDir + "/instances/" + cfg.instId;
    QString verDir = cfg.dataDir + "/versions/" + cfg.mcVer;
    QString runtimeDir = cfg.dataDir + "/runtimes";
    QDir().mkpath(instDir + "/mods");
    QDir().mkpath(verDir);
    QDir().mkpath(runtimeDir);

    QString jarPath = verDir + "/" + cfg.mcVer + ".jar";
    if (QFile::exists(jarPath) && QFileInfo(jarPath).size() > 1000000) {
        // Клиент уже скачан — сразу переходим к запуску
        startJavaProcess(jarPath, cfg, parentUi);
    } else {
        // Сначала скачиваем официальный клиент с серверов Mojang
        downloadVersionManifest(cfg, parentUi);
    }
}

void GameLauncher::downloadVersionManifest(const LaunchConfig &cfg, QWidget *parentUi) {
    auto *dlg = new QProgressDialog(
        "Получение манифеста версии " + cfg.mcVer + " от Mojang...",
        "Отмена", 0, 0, parentUi);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->show();

    QUrl manifestUrl("https://mojang.com");
    auto *rep = net->get(QNetworkRequest(manifestUrl));

    connect(rep, &QNetworkReply::finished, [this, rep, dlg, cfg, parentUi]() {
        dlg->close();
        dlg->deleteLater();
        if (rep->error() != QNetworkReply::NoError) {
            QMessageBox::warning(parentUi, "Ошибка сети",
                "Не удалось получить список версий Mojang:\n" + rep->errorString());
            rep->deleteLater();
            return;
        }

        auto doc = QJsonDocument::fromJson(rep->readAll());
        rep->deleteLater();
        auto versions = doc.object()["versions"].toArray();
        QString verJsonUrl;

        for (const auto &v : versions) {
            auto obj = v.toObject();
            if (obj["id"].toString() == cfg.mcVer) {
                verJsonUrl = obj["url"].toString();
                break;
            }
        }

        if (verJsonUrl.isEmpty()) {
            QMessageBox::warning(parentUi, "Ошибка",
                "Версия " + cfg.mcVer + " не найдена в манифесте Mojang.");
            return;
        }

        // Запрашиваем JSON конкретной версии для получения ссылки на client.jar
        auto *verRep = net->get(QNetworkRequest(QUrl(verJsonUrl)));
        connect(verRep, &QNetworkReply::finished, [this, verRep, cfg, parentUi]() {
            if (verRep->error() != QNetworkReply::NoError) {
                QMessageBox::warning(parentUi, "Ошибка", verRep->errorString());
                verRep->deleteLater();
                return;
            }
            auto vDoc = QJsonDocument::fromJson(verRep->readAll());
            verRep->deleteLater();

            QString clientUrl = vDoc.object()["downloads"].toObject()
                                    ["client"].toObject()["url"].toString();
            QString jarPath = cfg.dataDir + "/versions/" + cfg.mcVer + "/" + cfg.mcVer + ".jar";
            downloadClientJar(clientUrl, jarPath, cfg, parentUi);
        });
    });
}

void GameLauncher::downloadClientJar(const QString &clientUrl, const QString &jarPath,
                                     const LaunchConfig &cfg, QWidget *parentUi) {
    auto *dlg = new QProgressDialog(
        "Загрузка Minecraft " + cfg.mcVer + " (client.jar)...",
        "Отмена", 0, 100, parentUi);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->show();

    auto *rep = net->get(QNetworkRequest(QUrl(clientUrl)));

    connect(rep, &QNetworkReply::downloadProgress, [dlg](qint64 recv, qint64 total) {
        if (total > 0) {
            dlg->setValue(static_cast<int>((recv * 100) / total));
        }
    });

    connect(rep, &QNetworkReply::finished, [this, rep, dlg, jarPath, cfg, parentUi]() {
        dlg->close();
        dlg->deleteLater();
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
    QString neededJava = selectJavaVersion(cfg.mcVer);
    QString javaBin = cfg.dataDir + "/runtimes/bin/java";
    bool hasJava = QFile::exists(javaBin);

    QString report =
        "Сборка: " + cfg.instName + "\n" +
        "Клиент: " + jarPath + " (Скачан!)\n" +
        "Загрузчик: " + cfg.loader + " " + cfg.loaderVer + "\n" +
        "Требуемая VM: " + neededJava + (hasJava ? " [Установлена]" : " [Не распакована]") + "\n" +
        "Выделено RAM: " + QString::number(cfg.ramMb) + " МБ\n" +
        "Никнейм: " + cfg.nick;

    QMessageBox::information(parentUi, "Готово к запуску в JVM", report);
}
