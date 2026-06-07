add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.kde.latte.plasmoid\")

plasma_install_package(package org.kde.latte.plasmoid)

set(tasks_SRCS
    plugin/types.cpp
    plugin/lattetasksplugin.cpp
    plugin/smartlauncherbackend.cpp
    plugin/smartlauncheritem.cpp
)

add_library(lattetasksplugin SHARED ${tasks_SRCS})

target_link_libraries(lattetasksplugin
                      Qt6::Core
                      Qt6::Qml
                      Qt6::DBus
                      Plasma::Plasma
                      KF6::Svg
                      KF6::Service
                      KF6::ConfigCore
                      PW::LibNotificationManager
                      Plasma::PlasmaQuick)
                      
install(TARGETS lattetasksplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/latte/private/tasks)
install(FILES plugin/qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/latte/private/tasks)
