#pragma once
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QProcess>
#include "GameLauncher.h"
#include "TouchControls.h"

class JvmRunner {
public:
    static void run(QNetworkAccessManager *net, const QString &jp, const LaunchConfig &cfg, QWidget *ui) {
        QString jDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/jre21";
        QString jBin = jDir + "/bin/java";
        if (QFile::exists(jBin)) { execJvm(jBin, jp, cfg, ui); return; }

        auto *dlg = new QProgressDialog("Установка Java 21 ARM64...", "Отмена", 0, 100, ui);
        dlg->setWindowModality(Qt::WindowModal); dlg->show();
        QNetworkRequest req(QUrl("https://adoptium.net"));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        auto *rep = net->get(req);
        QObject::connect(rep, &QNetworkReply::downloadProgress, [dlg](qint64 r, qint64 t) { if (t > 0) dlg->setValue(int(r * 100 / t)); });
        QObject::connect(rep, &QNetworkReply::finished, [rep, dlg, jDir, jBin, jp, cfg, ui]() {
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
            execJvm(QFile::exists(jBin) ? jBin : "/system/bin/app_process64", jp, cfg, ui);
        });
    }

private:
    static void execJvm(const QString &jBin, const QString &jp, const LaunchConfig &cfg, QWidget *ui) {
        QString glLib = "libgl4es_114.so";
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (cfg.renderer == "Zink") { glLib = "libOSMesa.so"; env.insert("GALLIUM_DRIVER", "zink"); }
        else if (cfg.renderer == "Freedreno") { glLib = "libOSMesa.so"; env.insert("GALLIUM_DRIVER", "freedreno"); }
        else if (cfg.renderer == "LTW") glLib = "libltw.so";
        else if (cfg.renderer == "OpenGL") glLib = "libGLESv2.so";
        else { env.insert("LIBGL_ES", "2"); env.insert("LIBGL_MIPMAP", "3"); }

        QString natPath = QCoreApplication::applicationDirPath() + ":" + cfg.dataDir + "/natives";
        env.insert("LD_LIBRARY_PATH", natPath);

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

        QStringList args = {"-Xmx" + QString::number(cfg.ramMb) + "M",
                            "-Djava.library.path=" + natPath, "-Dorg.lwjgl.librarypath=" + natPath,
                            "-Dorg.lwjgl.opengl.libname=" + glLib, "-cp", cp.join(":"),
                            "net.minecraft.client.main.Main", "--username", cfg.nick,
                            "--version", cfg.mcVer.trimmed(), "--gameDir", gDir,
                            "--assetsDir", cfg.dataDir + "/assets", "--uuid", cfg.uuid, "--accessToken", cfg.token};

        auto *p = new QProcess(ui);
        p->setProcessEnvironment(env);
        p->setWorkingDirectory(gDir);
        p->start(jBin, args);

        TouchOverlay ov(cfg.mcVer + " [" + cfg.renderer + "] | " + cfg.nick, ui);
        ov.exec();
        if (p->state() != QProcess::NotRunning) p->kill();
    }
};
