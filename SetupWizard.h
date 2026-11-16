#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include "AssetManager.h"

class SetupWizardDialog : public QDialog {
public:
    static bool isCompleted(const QString &baseDir) {
        return QFile::exists(baseDir + "/setup_done.flag");
    }

    SetupWizardDialog(const QString &baseDir, QNetworkAccessManager *net, QWidget *parent)
        : QDialog(parent), dir(baseDir), nam(net) {
        setWindowTitle("Установка компонентов Prism Launcher");
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        showFullScreen();
        setStyleSheet("background:#181818; color:#FFFFFF;");

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(32, 28, 32, 28);
        lay->setSpacing(14);

        auto *title = new QLabel("Первоначальная настройка Prism Launcher");
        title->setStyleSheet("font-size:20px; font-weight:bold; color:#FFFFFF;");
        lay->addWidget(title);

        auto *desc = new QLabel(
            "Для запуска Minecraft на Android необходимо скачать базовые игровые компоненты:\n\n"
            "• Метаданные версий и конфигурация\n"
            "• Библиотеки Minecraft (Libraries) и профили рендеров (GL4ES, Zink, Freedreno, LTW)\n"
            "• Игровые ресурсы Minecraft Assets (текстуры, звуки, языки)\n"
            "• Виртуальная машина Java Runtime (ARM64 OpenJDK 21)\n\n"
            "Загрузка займёт 2–3 минуты. После завершения откроется главное меню лаунчера.");
        desc->setWordWrap(true);
        desc->setStyleSheet("font-size:14px; color:#CCCCCC; line-height:1.4;");
        lay->addWidget(desc);

        lay->addStretch();

        statusLbl = new QLabel("Готово к загрузке. Нажмите кнопку «Скачать» внизу.");
        statusLbl->setStyleSheet("font-size:13px; color:#56CCF2;");
        lay->addWidget(statusLbl);

        bar = new QProgressBar();
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setFixedHeight(22);
        bar->setStyleSheet(
            "QProgressBar { background:#2A2A2A; border-radius:6px; text-align:center; color:white; }"
            "QProgressBar::chunk { background:#2B7CD3; border-radius:6px; }");
        lay->addWidget(bar);

        btnStart = new QPushButton("⬇️  Скачать");
        btnStart->setFixedHeight(48);
        btnStart->setStyleSheet(
            "background:#2B7CD3; color:white; font-size:16px; font-weight:bold; border-radius:8px;");
        lay->addWidget(btnStart);

        connect(btnStart, &QPushButton::clicked, this, &SetupWizardDialog::startDownload);
    }

private:
    QString dir;
    QNetworkAccessManager *nam;
    QLabel *statusLbl;
    QProgressBar *bar;
    QPushButton *btnStart;

    void startDownload() {
        btnStart->setEnabled(false);
        btnStart->setText("Идёт загрузка компонентов...");
        statusLbl->setText("Этап 1/3: Получение манифеста и базовых ресурсов (1.21.4)...");
        bar->setValue(10);

        auto *rep = nam->get(QNetworkRequest(QUrl("https://mojang.com")));
        connect(rep, &QNetworkReply::finished, [this, rep]() {
            if (rep->error() != QNetworkReply::NoError) {
                statusLbl->setText("Ошибка сети: " + rep->errorString());
                btnStart->setEnabled(true);
                btnStart->setText("Повторить загрузку");
                rep->deleteLater();
                return;
            }
            auto arr = QJsonDocument::fromJson(rep->readAll()).object()["versions"].toArray();
            rep->deleteLater();

            QString verUrl;
            for (const auto &v : arr) {
                if (v.toObject()["id"].toString() == "1.21.4") {
                    verUrl = v.toObject()["url"].toString();
                    break;
                }
            }
            if (verUrl.isEmpty()) { finishAll(); return; }

            auto *vRep = nam->get(QNetworkRequest(QUrl(verUrl)));
            connect(vRep, &QNetworkReply::finished, [this, vRep]() {
                if (vRep->error() != QNetworkReply::NoError) { vRep->deleteLater(); finishAll(); return; }
                QByteArray raw = vRep->readAll();
                vRep->deleteLater();

                statusLbl->setText("Этап 2/3: Загрузка Minecraft Assets (текстуры и звуки)...");
                bar->setValue(35);

                AssetManager::downloadAssets(nam, raw, dir, "1.21.4", this, [this, raw]() {
                    downloadLibsAndJava(raw);
                });
            });
        });
    }

    void downloadLibsAndJava(const QByteArray &raw) {
        statusLbl->setText("Этап 2/3: Загрузка библиотек игры (Libraries)...");
        bar->setValue(60);

        auto doc = QJsonDocument::fromJson(raw).object();
        QList<QPair<QString, QString>> libs;
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
            if (!art["url"].toString().isEmpty() && !art["path"].toString().isEmpty()) {
                libs.append({art["url"].toString(), dir + "/libraries/" + art["path"].toString()});
            }
        }

        AssetManager::downloadBatch(nam, "Загрузка библиотек", libs, this, [this]() {
            installJavaRuntime();
        });
    }

    void installJavaRuntime() {
        statusLbl->setText("Этап 3/3: Установка виртуальной машины Java 21 ARM64...");
        bar->setValue(80);

        QString jDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/jre21";
        QString jBin = jDir + "/bin/java";
        if (QFile::exists(jBin)) { finishAll(); return; }

        QNetworkRequest req(QUrl("https://adoptium.net"));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        auto *rep = nam->get(req);

        connect(rep, &QNetworkReply::downloadProgress, [this](qint64 r, qint64 t) {
            if (t > 0) bar->setValue(80 + int((r * 20) / t));
        });

        connect(rep, &QNetworkReply::finished, [this, rep, jDir]() {
            if (rep->error() == QNetworkReply::NoError) {
                QDir().mkpath(jDir);
                QString tp = jDir + "/jre.tar.gz";
                QFile f(tp);
                if (f.open(QIODevice::WriteOnly)) { f.write(rep->readAll()); f.close(); }
                QProcess::execute("tar", {"-xzf", tp, "-C", jDir, "--strip-components=1"});
                QFile::remove(tp);
                QProcess::execute("chmod", {"-R", "755", jDir + "/bin"});
            }
            rep->deleteLater();
            finishAll();
        });
    }

    void finishAll() {
        bar->setValue(100);
        statusLbl->setText("Все компоненты установлены! Переход в главное меню...");
        QFile flag(dir + "/setup_done.flag");
        if (flag.open(QIODevice::WriteOnly)) { flag.write("OK"); flag.close(); }
        accept(); // Автоматически закрывает окно и перекидывает в главное меню с кнопкой Launch
    }
};
