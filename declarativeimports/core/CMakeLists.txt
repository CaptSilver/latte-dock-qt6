set(lattecoreplugin_SRCS
    lattecoreplugin.cpp
    dialog.cpp
    environment.cpp
    iconitem.cpp
    quickwindowsystem.cpp
    tools.cpp
    types.h
)

add_library(lattecoreplugin SHARED ${lattecoreplugin_SRCS})

target_link_libraries(lattecoreplugin
    Qt6::Quick
    Qt6::Qml
    KF6::CoreAddons
    Plasma::Plasma
    KF6::Svg
    KF6::Package
    Plasma::PlasmaQuick
    # KF5::QuickAddons has no KF6 C++ target; kquickcontrolsaddons is QML-only now.
    KF6::IconThemes
    KF6::WindowSystem
)

install(TARGETS lattecoreplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/latte/core)

install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/latte/core)
