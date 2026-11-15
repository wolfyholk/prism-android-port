#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QMessageBox>
#include <QInputDialog>
#include <QStyleFactory>

struct InstanceItem {
    QString name;
    QString version;
    QString category;
    QString colorHex;
    QString badge;
};

class PrismMainWindow : public QMainWindow {
public:
    PrismMainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Prism Launcher 9.4");
        resize(1024, 640);
        applyDarkTheme();

        instances = {
            {"Fabric 1.16.5", "1.16.5", "Ungrouped", "#36C5F0", "F"},
            {"Forge 1.12.2", "1.12.2", "Ungrouped", "#D93829", "TNT"},
            {"NeoForge 1.21.1", "1.21.1", "Ungrouped", "#E0E0E0", "NF"},
            {"Quilt 1.18.2", "1.18.2", "Ungrouped", "#F2C94C", "Q"},
            {"Snapshot 26.1", "26.1", "Ungrouped", "#795548", "26.1"},
            {"Vanilla 1.21.11", "1.21.11", "Ungrouped", "#6E7F80", "MC"},
            {"All the Mods 10", "1.21.1", "Modpack", "#F2994A", "ATM10"},
            {"Better MC", "1.20.1", "Modpack", "#56CCF2", "BMC"},
            {"Cobblemon", "1.20.1", "Modpack", "#EB5757", "CB"},
            {"SkyFactory 4", "1.12.2", "Modpack", "#6FCF97", "SF4"},
            {"The Pixelmon Modpack", "1.16.5", "Modpack", "#BB6BD9", "PM"},
            {"Additive", "1.21.4", "Vanilla-like", "#FF7675", "U"},
            {"Adrenaline", "1.21.4", "Vanilla-like", "#A3CB38", "AD"},
            {"Fabulously Optimized", "1.21.4", "Vanilla-like", "#C4B581", "FO"}
        };

        QWidget *central = new QWidget(this);
        QVBoxLayout *rootLayout = new QVBoxLayout(central);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // 1. ВЕРХНЯЯ ПАНЕЛЬ ИНСТРУМЕНТОВ
        QWidget *topBar = new QWidget();
        topBar->setFixedHeight(44);
        topBar->setStyleSheet("background-color: #2B2B2B; border-bottom: 1px solid #141414;");
        QHBoxLayout *topLayout = new QHBoxLayout(topBar);
        topLayout->setContentsMargins(8, 4, 8, 4);
        topLayout->setSpacing(6);

        QPushButton *btnAdd = createTopBtn("➕ Add Instance");
        QPushButton *btnFolders = createTopBtn("📁 Folders ▾");
        QPushButton *btnSettings = createTopBtn("⚙️ Settings");
        QPushButton *btnHelp = createTopBtn("❓ Help ▾");
        QPushButton *btnAccounts = createTopBtn("🧑 Accounts ▾");

        connect(btnAdd, &QPushButton::clicked, this, &PrismMainWindow::onAddInstance);
        connect(btnSettings, &QPushButton::clicked, [this]() {
            QMessageBox::information(this, "Settings", "Настройки Java (JRE 17/21), памяти RAM и рендера (GL4ES / Zink).");
        });
        connect(btnAccounts, &QPushButton::clicked, [this]() {
            QMessageBox::information(this, "Accounts", "Управление аккаунтами Microsoft и Offline-профилями.");
        });

        topLayout->addWidget(btnAdd);
        topLayout->addWidget(btnFolders);
        topLayout->addWidget(btnSettings);
        topLayout->addWidget(btnHelp);
        topLayout->addStretch();
        topLayout->addWidget(btnAccounts);
        rootLayout->addWidget(topBar);

        // 2. ЦЕНТРАЛЬНАЯ ОБЛАСТЬ
        QHBoxLayout *middleLayout = new QHBoxLayout();
        middleLayout->setContentsMargins(0, 0, 0, 0);
        middleLayout->setSpacing(0);

        QScrollArea *scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setStyleSheet("background-color: #1E1E1E;");

        gridContainer = new QWidget();
        gridContainer->setStyleSheet("background-color: #1E1E1E;");
        gridMainLayout = new QVBoxLayout(gridContainer);
        gridMainLayout->setContentsMargins(12, 10, 12, 10);
        gridMainLayout->setSpacing(14);

        scrollArea->setWidget(gridContainer);
        middleLayout->addWidget(scrollArea, 1);

        // Правая боковая панель
        QWidget *rightPanel = new QWidget();
        rightPanel->setFixedWidth(185);
        rightPanel->setStyleSheet("background-color: #252525; border-left: 1px solid #141414;");
        QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(10, 14, 10, 10);
        rightLayout->setSpacing(4);

        selectedIconLabel = new QLabel("MC");
        selectedIconLabel->setFixedSize(68, 68);
        selectedIconLabel->setAlignment(Qt::AlignCenter);
        selectedIconLabel->setStyleSheet("background-color: #6E7F80; border-radius: 8px; font-weight: bold; font-size: 18px; color: white;");

        selectedNameLabel = new QLabel("Vanilla 1.21.11");
        selectedNameLabel->setAlignment(Qt::AlignCenter);
        selectedNameLabel->setWordWrap(true);
        selectedNameLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #EAEAEA; margin-bottom: 8px;");

        rightLayout->addWidget(selectedIconLabel, 0, Qt::AlignHCenter);
        rightLayout->addWidget(selectedNameLabel);

        QPushButton *btnLaunch = createSideBtn("▶️  Launch", true);
        QPushButton *btnKill = createSideBtn("✖️  Kill", false);
        btnKill->setEnabled(false);

        connect(btnLaunch, &QPushButton::clicked, [this]() {
            QMessageBox::information(this, "Запуск Minecraft",
                "Запуск сборки: " + selectedNameLabel->text() +
                "\nСледующий шаг: подключение ARM64 Java Runtime и игрового окна с сенсорным управлением.");
        });

        rightLayout->addWidget(btnLaunch);
        rightLayout->addWidget(btnKill);
        rightLayout->addSpacing(6);
        rightLayout->addWidget(createSideBtn("📝  Edit", false));
        rightLayout->addWidget(createSideBtn("🏷️  Change Group", false));
        rightLayout->addWidget(createSideBtn("📂  Folder", false));
        rightLayout->addWidget(createSideBtn("📤  Export", false));
        rightLayout->addWidget(createSideBtn("📋  Copy", false));
        rightLayout->addWidget(createSideBtn("🗑️  Delete", false));
        rightLayout->addWidget(createSideBtn("🔗  Create Shortcut", false));
        rightLayout->addStretch();

        middleLayout->addWidget(rightPanel);
        rootLayout->addLayout(middleLayout, 1);

        // 3. НИЖНЯЯ ПАНЕЛЬ НОВОСТЕЙ И СТАТУСА
        QWidget *newsBar = new QWidget();
        newsBar->setFixedHeight(32);
        newsBar->setStyleSheet("background-color: #1E1E1E; border-top: 1px solid #2D2D2D;");
        QHBoxLayout *newsLayout = new QHBoxLayout(newsBar);
        newsLayout->setContentsMargins(12, 2, 12, 2);

        QLabel *newsText = new QLabel("💬  Minecraft Java Edition — Prism Launcher Android Port");
        newsText->setStyleSheet("color: #CCCCCC; font-size: 12px;");
        QPushButton *moreNewsBtn = new QPushButton("💬 More news");
        moreNewsBtn->setStyleSheet("background: transparent; color: #CCCCCC; border: none; font-size: 12px;");

        newsLayout->addStretch();
        newsLayout->addWidget(newsText);
        newsLayout->addStretch();
        newsLayout->addWidget(moreNewsBtn);
        rootLayout->addWidget(newsBar);

        QWidget *bottomStatus = new QWidget();
        bottomStatus->setFixedHeight(26);
        bottomStatus->setStyleSheet("background-color: #252525; border-top: 1px solid #141414;");
        QHBoxLayout *statusLayout = new QHBoxLayout(bottomStatus);
        statusLayout->setContentsMargins(10, 0, 10, 0);

        statusLeftLabel = new QLabel("Minecraft 1.21.11");
        statusLeftLabel->setStyleSheet("color: #BBBBBB; font-size: 11px;");
        QLabel *statusRightLabel = new QLabel("Total playtime: 0s");
        statusRightLabel->setStyleSheet("color: #BBBBBB; font-size: 11px;");

        statusLayout->addWidget(statusLeftLabel);
        statusLayout->addStretch();
        statusLayout->addWidget(statusRightLabel);
        rootLayout->addWidget(bottomStatus);

        setCentralWidget(central);
        rebuildGrid("Vanilla 1.21.11");
    }

private:
    QList<InstanceItem> instances;
    QWidget *gridContainer;
    QVBoxLayout *gridMainLayout;
    QLabel *selectedIconLabel;
    QLabel *selectedNameLabel;
    QLabel *statusLeftLabel;

    void applyDarkTheme() {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(30, 30, 30));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        qApp->setPalette(darkPalette);
    }

    QPushButton* createTopBtn(const QString &text) {
        QPushButton *btn = new QPushButton(text);
        btn->setStyleSheet(
            "QPushButton { background-color: transparent; color: #EAEAEA; border: none; padding: 6px 10px; border-radius: 4px; font-size: 13px; }"
            "QPushButton:hover { background-color: #3A3A3A; }"
            "QPushButton:pressed { background-color: #4A4A4A; }"
        );
        return btn;
    }

    QPushButton* createSideBtn(const QString &text, bool primary) {
        QPushButton *btn = new QPushButton(text);
        btn->setStyleSheet(
            "QPushButton { text-align: left; background-color: transparent; color: #EAEAEA; border: none; padding: 7px 8px; border-radius: 4px; font-size: 13px; }"
            "QPushButton:hover { background-color: #353535; }"
            "QPushButton:disabled { color: #666666; }"
        );
        if (primary) {
            btn->setStyleSheet(
                "QPushButton { text-align: left; background-color: #2E3436; color: #FFFFFF; font-weight: bold; border: 1px solid #3E4446; padding: 8px 8px; border-radius: 4px; font-size: 13px; }"
