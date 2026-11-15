#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

struct LaunchConfig {
    QString instId;
    QString instName;
    QString mcVer;
    QString loader;
    QString loaderVer;
    QString nick;
    QString dataDir;
    int ramMb = 2048;
};

class GameLauncher : public QObject {
public:
    explicit GameLauncher(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Главный метод подготовки и запуска версии
    void prepareAndLaunch(const LaunchConfig &cfg, QWidget *parentUi);

private:
    QNetworkAccessManager *net;
    QString selectJavaVersion(const QString &mcVer);
    void downloadVersionManifest(const LaunchConfig &cfg, QWidget *parentUi);
    void downloadClientJar(const QString &clientUrl, const QString &jarPath,
                           const LaunchConfig &cfg, QWidget *parentUi);
    void startJavaProcess(const QString &jarPath, const LaunchConfig &cfg,
                          QWidget *parentUi);
};
