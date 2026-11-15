#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QPixmap>
#include <QImage>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include "AccountManager.h"

class SkinManager {
public:
    static void openSkinDialog(const QString &baseDir, const AccountInfo &acc,
                               QNetworkAccessManager *net, QWidget *parent) {
        QDir().mkpath(baseDir + "/skins");
        QDialog dlg(parent);
        dlg.setWindowTitle("Смена скина (PNG) - " + acc.name);
        dlg.resize(420, 340);
        auto *lay = new QVBoxLayout(&dlg);

        auto *info = new QLabel("Аккаунт: " + acc.name +
            (acc.type == "MSA" ? " (Лицензия Mojang)" : " (Offline профиль)"));
        info->setStyleSheet("font-weight:bold; font-size:14px;");
        lay->addWidget(info);

        auto *preview = new QLabel("Текстура PNG не выбрана");
        preview->setFixedSize(160, 160);
        preview->setAlignment(Qt::AlignCenter);
        preview->setStyleSheet("background:#252525; border:1px solid #444; border-radius:8px;");

        QString savedSkin = baseDir + "/skins/" + acc.name + ".png";
        if (QFile::exists(savedSkin)) {
            QPixmap pm(savedSkin);
            preview->setPixmap(pm.scaled(140, 140, Qt::KeepAspectRatio, Qt::FastTransformation));
        }
        lay->addWidget(preview, 0, Qt::AlignHCenter);

        auto *hModel = new QHBoxLayout();
        hModel->addWidget(new QLabel("Модель рук:"));
        auto *cbModel = new QComboBox();
        cbModel->addItems({"classic (Steve 4px)", "slim (Alex 3px)"});
        hModel->addWidget(cbModel);
        lay->addLayout(hModel);

        QString selectedPngPath = QFile::exists(savedSkin) ? savedSkin : "";

        auto *bPick = new QPushButton("📂 Выбрать PNG-файл скина...");
        auto *bApply = new QPushButton("✅ Применить скин");
        bApply->setStyleSheet("background:#2B7CD3; color:#fff; font-weight:bold; padding:8px;");

        QObject::connect(bPick, &QPushButton::clicked, [&]() {
            QString path = QFileDialog::getOpenFileName(
                &dlg, "Выберите скин (PNG)", "/storage/emulated/0", "PNG Images (*.png)");
            if (path.isEmpty()) return;

            QImage img(path);
            if (img.isNull() || (img.width() != 64 && img.width() != 128)) {
                QMessageBox::warning(&dlg, "Неверный формат",
                    "Выберите стандартную PNG-текстуру скина Minecraft (64x64 или 64x32).");
                return;
            }
            selectedPngPath = path;
            preview->setPixmap(QPixmap::fromImage(img).scaled(
                140, 140, Qt::KeepAspectRatio, Qt::FastTransformation));
        });

        QObject::connect(bApply, &QPushButton::clicked, [&]() {
            if (selectedPngPath.isEmpty()) {
                QMessageBox::warning(&dlg, "Ошибка", "Сначала выберите PNG-файл!");
                return;
            }
            // Копируем скин в PrismLauncher/skins/<ник>.png
            if (selectedPngPath != savedSkin) {
                QFile::remove(savedSkin);
                QFile::copy(selectedPngPath, savedSkin);
            }

            QString variant = cbModel->currentIndex() == 1 ? "slim" : "classic";

            // Если лицензия Microsoft — загружаем скин напрямую в Mojang API
            if (acc.type == "MSA" && acc.token != "0" && !acc.token.isEmpty()) {
                auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
                QHttpPart vPart;
                vPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                QVariant("form-data; name=\"variant\""));
                vPart.setBody(variant.toUtf8());

                QHttpPart fPart;
                fPart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/png"));
                fPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                QVariant("form-data; name=\"file\"; filename=\"skin.png\""));
                auto *f = new QFile(savedSkin);
                f->open(QIODevice::ReadOnly);
                fPart.setBodyDevice(f);
                f->setParent(multi);

                multi->append(vPart);
                multi->append(fPart);

                QNetworkRequest req(QUrl("https://minecraftservices.com"));
                req.setRawHeader("Authorization", ("Bearer " + acc.token).toUtf8());
                auto *rep = net->post(req, multi);
                multi->setParent(rep);

                QObject::connect(rep, &QNetworkReply::finished, [&, rep]() {
                    if (rep->error() == QNetworkReply::NoError) {
                        QMessageBox::information(&dlg, "Успех", "Скин загружен на официальный сервер Mojang!");
                        dlg.accept();
                    } else {
                        QMessageBox::warning(&dlg, "Ошибка Mojang API", rep->errorString());
                    }
                    rep->deleteLater();
                });
            } else {
                // Для Offline-аккаунта создаем готовый ресурспак с заменой steve.png / alex.png
                QString rpDir = baseDir + "/skins/CustomSkinPack/assets/minecraft/textures/entity/player/" +
                                (variant == "slim" ? "slim" : "wide");
                QDir().mkpath(rpDir);
                QString targetTex = rpDir + (variant == "slim" ? "/alex.png" : "/steve.png");
                QFile::remove(targetTex);
                QFile::copy(savedSkin, targetTex);

                QFile mcmeta(baseDir + "/skins/CustomSkinPack/pack.mcmeta");
                if (mcmeta.open(QIODevice::WriteOnly)) {
                    mcmeta.write("{\"pack\":{\"pack_format\":15,\"description\":\"Prism Custom Skin\"}}");
                    mcmeta.close();
                }
                QMessageBox::information(&dlg, "Скин установлен",
                    "PNG-текстура сохранена в PrismLauncher/skins/ и подготовлена для Offline-игры!");
                dlg.accept();
            }
        });

        lay->addWidget(bPick);
        lay->addWidget(bApply);
        dlg.exec();
    }
};
