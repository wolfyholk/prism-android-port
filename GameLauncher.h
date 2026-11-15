#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPair>
#include <QNetworkAccessManager>

struct LaunchConfig {
    QString instId;
    QString instName;
    QString mcVer;
    QString loader;
    QString loaderVer;
    QString nick;
    QString accType;
    QString uuid;
    QString token;
    QString dataDir;
    int ramMb = 2048;
};

class GameLauncher : public QObject {
public:
    explicit GameLauncher(QNetworkAccessManager *nam, QObject *parent = nullptr);
    void prepareAndLaunch(const LaunchConfig &cfg, QWidget *parentUi);

private:
    QNetworkAccessManager *net;
    QList<QPair<QString, QString>> libQueue; // <URL, локальный путь в libraries/>
    int totalLibs = 0;

    QString selectJavaVersion(const QString &mcVer);
    void downloadVersionManifest(const LaunchConfig &cfg, QWidget *parentUi);
    void parseAndDownloadLibraries(const QByteArray &verJsonBytes, const LaunchConfig &cfg, QWidget *parentUi);
    void downloadNextLibrary(const QString &jarUrl, const LaunchConfig &cfg, QWidget *parentUi, class QProgressDialog *dlg);
    void downloadClientJar(const QString &clientUrl, const QString &jarPath,
                           const LaunchConfig &cfg, QWidget *parentUi);
    void startJavaProcess(const QString &jarPath, const LaunchConfig &cfg,
                          QWidget *parentUi);
};
