#pragma once
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QJniObject>

namespace StorageManager {

    // Список обязательных папок оригинального Prism Launcher в корне устройства
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

    // Проверка разрешения и создание структуры папок
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
