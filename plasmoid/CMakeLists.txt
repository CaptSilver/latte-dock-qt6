add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.kde.latte.plasmoid\")

plasma_install_package(package org.kde.latte.plasmoid)

set(tasks_SRCS
    plugin/types.cpp
    plugin/lattetasksplugin.cpp
    plugin/smartlauncherbackend.cpp
    plugin/smartlauncheritem.cpp
    plugin/backend.cpp
)
kconfig_add_kcfg_files(tasks_SRCS plugin/kactivitymanagerd_plugins_settings.kcfgc)

add_library(lattetasksplugin SHARED ${tasks_SRCS})

target_link_libraries(lattetasksplugin
                      Qt6::Core
                      Qt6::Qml
                      Qt6::DBus
                      Plasma::Plasma
                      Plasma::PlasmaQuick
                      Plasma::Activities
                      Plasma::ActivitiesStats
                      KSysGuard::ProcessCore
                      KF6::Svg
                      KF6::Service
                      KF6::ConfigCore
                      KF6::ConfigGui
                      KF6::I18n
                      KF6::KIOCore
                      KF6::KIOGui
                      KF6::KIOFileWidgets
                      KF6::Notifications
                      KF6::WindowSystem
                      PW::LibNotificationManager)

target_include_directories(lattetasksplugin PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
                      
install(TARGETS lattetasksplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/latte/private/tasks)
install(FILES plugin/qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/latte/private/tasks)
