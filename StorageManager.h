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

        // Открываем общий список "Доступ ко всем файлам" (не вылетает на Xiaomi/HyperOS)
        QJniObject actStr = QJniObject::fromString(
            "android.settings.MANAGE_ALL_FILES_ACCESS_PERMISSION");
        QJniObject intent(
            "android/content/Intent",
            "(Ljava/lang/String;)V",
            actStr.object<jstring>());

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
                "Включите управление хранилищем: в открывшемся списке найдите "
                "PrismAndroidTest и разрешите доступ ко всем файлам."
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
