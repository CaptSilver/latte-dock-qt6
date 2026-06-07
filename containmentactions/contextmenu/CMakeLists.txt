add_definitions(-DTRANSLATION_DOMAIN=\"plasma_containmentactions_lattecontextmenu\")

set(contextmenu_SRCS
    generictools.cpp
    layoutmenuitemwidget.cpp
    menu.cpp
)

add_library(plasma_containmentactions_lattecontextmenu MODULE ${contextmenu_SRCS})

# KF6 derives a plugin's id from its file name (not the embedded KPlugin/Id), so the library must be
# named after the id the configs reference (org.kde.latte.contextmenu); otherwise libplasma's
# lookup-by-id fails and the right-click menu never loads.
set_target_properties(plasma_containmentactions_lattecontextmenu PROPERTIES
                      OUTPUT_NAME org.kde.latte.contextmenu)

target_link_libraries(plasma_containmentactions_lattecontextmenu
                        Qt6::DBus
                        Qt6::Widgets
                        KF6::I18n
                        KF6::XmlGui
                        Plasma::Plasma)

install(TARGETS plasma_containmentactions_lattecontextmenu DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/containmentactions)
