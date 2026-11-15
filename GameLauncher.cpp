#include "GameLauncher.h"
#include "TouchControls.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QMessageBox>
#include <QProcess>

GameLauncher::GameLauncher(QNetworkAccessManager *nam, QObject *parent) : QObject(parent), net(nam) {}

QString GameLauncher::selectJavaVersion(const QString &v) {
    return (v.startsWith("26.") || v.startsWith("1.21")) ? "21" : "17";
}

void GameLauncher::prepareAndLaunch(const LaunchConfig &cfg, QWidget *ui) {
    QString vDir = cfg.dataDir + "/versions/" + cfg.mcVer.trimmed();
    QDir().mkpath(cfg.dataDir + "/instances/" + cfg.instId + "/mods");
    QDir().mkpath(cfg.dataDir + "/libraries");
    QDir().mkpath(vDir);
    downloadVersionManifest(cfg, ui);
}

void GameLauncher::downloadVersionManifest(const LaunchConfig &cfg, QWidget *ui) {
    auto *dlg = new QProgressDialog("Поиск " + cfg.mcVer + "...", "Отмена", 0, 0, ui);
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
            if (vr->error() == QNetworkReply::NoError) parseAndDownloadLibraries(vr->readAll(), cfg, ui);
            vr->deleteLater();
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
        if (QFile::exists(jp) && QFileInfo(jp).size() > 1000000) ensureJavaAndRun(jp, cfg, ui);
        else downloadClientJar(jarUrl, jp, cfg, ui);
        return;
    }
    auto *dlg = new QProgressDialog("Библиотеки...", "Отмена", 0, totalLibs, ui);
    dlg->setWindowModality(Qt::WindowModal); dlg->show();
    downloadNextLibrary(jarUrl, cfg, ui, dlg);
}

void GameLauncher::downloadNextLibrary(const QString &jarUrl, const LaunchConfig &cfg, QWidget *ui, QProgressDialog *dlg) {
    if (dlg->wasCanceled() || libQueue.isEmpty()) {
        dlg->close(); dlg->deleteLater();
        QString jp = cfg.dataDir + "/versions/" + cfg.mcVer.trimmed() + "/client.jar";
        if (QFile::exists(jp) && QFileInfo(jp).size() > 1000000) ensureJavaAndRun(jp, cfg, ui);
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
            rep->deleteLater(); ensureJavaAndRun(jp, cfg, ui);
        } else rep->deleteLater();
    });
}

void GameLauncher::ensureJavaAndRun(const QString &jp, const LaunchConfig &cfg, QWidget *ui) {
    QString jDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/jre21";
    QString jBin = jDir + "/bin/java";
    if (QFile::exists(jBin)) { startJavaProcess(jBin, jp, cfg, ui); return; }

    auto *dlg = new QProgressDialog("Установка Java 21 ARM64...", "Отмена", 0, 100, ui);
    dlg->setWindowModality(Qt::WindowModal); dlg->show();
    QNetworkRequest req(QUrl("https://adoptium.net"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *rep = net->get(req);
    connect(rep, &QNetworkReply::downloadProgress, [dlg](qint64 r, qint64 t) { if (t > 0) dlg->setValue(int(r * 100 / t)); });
    connect(rep, &QNetworkReply::finished, [this, rep, dlg, jDir, jBin, jp, cfg, ui]() {
        dlg->close(); dlg->deleteLater();
        if (rep->error() == QNetworkReply::NoError) {
            QDir().mkpath(jDir);
            QString tp = jDir + "/jre.tar.gz";
            QFile f(tp); if (f.open(QIODevice::WriteOnly)) { f.write(rep->readAll()); f.close(); }
            QProcess::execute("tar", {"-xzf", tp, "-C", jDir, "--strip-components=1"});
            QFile::remove(tp);
            QProcess::execute("chmod", {"-R", "755", jDir + "/bin"});
        }
        rep->deleteLater();
        startJavaProcess(QFile::exists(jBin) ? jBin : "/system/bin/app_process64", jp, cfg, ui);
    });
}

void GameLauncher::startJavaProcess(const QString &jBin, const QString &jp, const LaunchConfig &cfg, QWidget *ui) {
    QStringList cp;
    QList<QDir> st = {QDir(cfg.dataDir + "/libraries")};
    while (!st.isEmpty()) {
        QDir c = st.takeFirst();
        for (const auto &fi : c.entryInfoList({"*.jar"}, QDir::Files)) cp.append(fi.absoluteFilePath());
        for (const auto &sd : c.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) st.append(QDir(sd.absoluteFilePath()));
    }
    cp.append(jp);
    QString gDir = cfg.dataDir + "/instances/" + cfg.instId + "/.minecraft";
    QDir().mkpath(gDir + "/resourcepacks");

    QStringList args = {"-Xmx" + QString::number(cfg.ramMb) + "M", "-Dorg.lwjgl.opengl.libname=libgl4es_114.so",
                        "-cp", cp.join(":"), "net.minecraft.client.main.Main", "--username", cfg.nick,
                        "--version", cfg.mcVer.trimmed(), "--gameDir", gDir, "--assetsDir", cfg.dataDir + "/assets",
                        "--uuid", cfg.uuid, "--accessToken", cfg.token};
    auto *p = new QProcess(ui);
    p->setWorkingDirectory(gDir);
    p->start(jBin, args);

    TouchOverlay ov(cfg.mcVer + " | " + cfg.nick, ui);
    ov.exec();
    if (p->state() != QProcess::NotRunning) p->kill();
}
