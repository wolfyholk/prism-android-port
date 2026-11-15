#pragma once
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QJniObject>
#include <QCoreApplication>

namespace StorageManager {

    inline QStringList requiredFolders() {
        return {
            "instances",
            "libraries",
            "assets",
            "icons",
            "java",
            "logs",
            "mods",
            "themes",
            "translations"
        };
    }

    inline void openStorageSettings() {
        QJniObject ctx = QNativeInterface::QAndroidApplication::context();
        if (!ctx.isValid()) return;

        QJniObject pkg = QJniObject::fromString("package:org.prism.androidtest");
        QJniObject uri = QJniObject::callStaticObjectMethod(
            "android/net/Uri", "parse",
            "(Ljava/lang/String;)Landroid/net/Uri;",
            pkg.object<jstring>());

        QJniObject actStr = QJniObject::fromString(
            "android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION");
        QJniObject intent(
            "android/content/Intent",
            "(Ljava/lang/String;Landroid/net/Uri;)V",
            actStr.object<jstring>(), uri.object());

        intent.callObjectMethod(
            "addFlags", "(I)Landroid/content/Intent;", 0x10000000);
        ctx.callMethod<void>(
            "startActivity", "(Landroid/content/Intent;)V", intent.object());
    }

    inline bool ensureReady(const QString &baseDir, QWidget *parent) {
        bool hasAccess = QJniObject::callStaticMethod<jboolean>(
            "android/os/Environment", "isExternalStorageManager", "()Z");

        if (!hasAccess) {
            QMessageBox::information(
                parent,
                "Требуется разрешение",
                "Включите управление хранилищем: зайдите в настройки и разрешения приложения, "
                "затем разрешите управление хранилищем."
            );
            openStorageSettings();
            return false;
        }

        for (const QString &folder : requiredFolders()) {
            QDir().mkpath(baseDir + "/" + folder);
        }

        QFile cfg(baseDir + "/prismlauncher.cfg");
        if (!cfg.exists() && cfg.open(QIODevice::WriteOnly)) {
            cfg.write("[General]\nConfigVersion=1.2\nUseNativeOpenAL=true\n");
            cfg.close();
        }
        return true;
    }
}
