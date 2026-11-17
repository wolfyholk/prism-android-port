cmake_minimum_required(VERSION 3.16)
project(PrismAndroidTest VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Core)
qt_standard_project_setup()

qt_add_executable(PrismAndroidTest
    MANUAL_FINALIZATION
    main.cpp
    StorageManager.h
    TouchControls.h
    AccountManager.h
    SkinManager.h
    VersionPicker.h
    AssetManager.h
    JvmRunner.h
    SetupWizard.h
    "manifest minecraft/ManifestClientLoader.h"
    GameLauncher.h
    GameLauncher.cpp
)

target_link_libraries(PrismAndroidTest PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Core
)

set(SSL_ROOT "${CMAKE_SOURCE_DIR}/openssl/ssl_3/arm64-v8a")

set_target_properties(PrismAndroidTest PROPERTIES
    QT_ANDROID_PACKAGE_NAME "org.prism.androidtest"
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android_pkg"
    QT_ANDROID_MIN_SDK_VERSION "26"
    QT_ANDROID_TARGET_SDK_VERSION "33"
    QT_ANDROID_EXTRA_LIBS "${SSL_ROOT}/libcrypto_3.so;${SSL_ROOT}/libssl_3.so"
)

qt_finalize_executable(PrismAndroidTest)
