#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QPair>
#include <QNetworkAccessManager>

struct LaunchConfig {
    QString instId, instName, mcVer, loader, loaderVer;
    QString nick, accType, uuid, token, dataDir;
    int ramMb = 2048;
    QString renderer = "Holy GL4ES";
};

class GameLauncher : public QObject {
public:
    explicit GameLauncher(QNetworkAccessManager *nam, QObject *parent = nullptr);
    void prepareAndLaunch(const LaunchConfig &cfg, QWidget *parentUi);

private:
    QNetworkAccessManager *net;
    QList<QPair<QString, QString>> libQueue;
    int totalLibs = 0;

    QString selectJavaVersion(const QString &mcVer);
    void downloadVersionManifest(const LaunchConfig &cfg, QWidget *parentUi);
    void parseAndDownloadLibraries(const QByteArray &raw, const LaunchConfig &cfg, QWidget *parentUi);
    void downloadNextLibrary(const QString &jarUrl, const LaunchConfig &cfg, QWidget *parentUi, class QProgressDialog *dlg);
    void downloadClientJar(const QString &url, const QString &jarPath, const LaunchConfig &cfg, QWidget *parentUi);
    void ensureJavaAndRun(const QString &jarPath, const LaunchConfig &cfg, QWidget *parentUi);
    void startJavaProcess(const QString &javaBin, const QString &jarPath, const LaunchConfig &cfg, QWidget *parentUi);
};
