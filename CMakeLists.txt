project(lattedock)
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(VERSION 0.10.77)
set(AUTHOR "Michail Vourlakos, Smith Ar")
set(EMAIL "mvourlakos@gmail.com, audoban@openmailbox.org")
set(WEBSITE "https://userbase.kde.org/LatteDock")
set(BUG_ADDRESS "https://bugs.kde.org/enter_bug.cgi?product=lattedock")
set(FAQS "https://userbase.kde.org/LatteDock/FAQ")

set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "6.5.0")
set(KDE_COMPILERSETTINGS_LEVEL "6.5.0")

set(KF6_LOCALE_PREFIX "")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(CheckIncludeFiles)
include(CMakePackageConfigHelpers)
include(ECMOptionalAddSubdirectory)
include(ECMQtDeclareLoggingCategory)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEPackageAppTemplates)
include(WriteBasicConfigVersionFile)


find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS DBus Gui Qml Quick Widgets WaylandClient Test)

# KF6 frameworks (Fedora 44 has no umbrella KF6 config -> find each individually)
find_package(KF6Archive ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6CoreAddons ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6GuiAddons ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6Crash ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6DBusAddons ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6GlobalAccel ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6I18n ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6IconThemes ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6KIO ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6NewStuff ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6Notifications ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6WindowSystem ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6XmlGui ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6Package ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6Config ${KF6_MIN_VERSION} REQUIRED)
# Transitively required by Plasma::Plasma's link interface (configure fails without it).
find_package(KF6KirigamiPlatform ${KF6_MIN_VERSION} REQUIRED)

# De-umbrella'd in Plasma 6 -> their own find_package calls
find_package(Plasma ${KF6_MIN_VERSION} REQUIRED)
find_package(PlasmaQuick ${KF6_MIN_VERSION} REQUIRED)
find_package(PlasmaActivities ${KF6_MIN_VERSION} REQUIRED)
find_package(KF6Service ${KF6_MIN_VERSION} REQUIRED)
# Launcher badge/progress in the tasks plugin reads NotificationManager job data.
find_package(LibNotificationManager REQUIRED)
# Recent-document and jump-list actions in the tasks backend query the activity store.
find_package(PlasmaActivitiesStats ${KF6_MIN_VERSION} REQUIRED)
# parentPid() resolves a process tree to match audio streams to tasks.
find_package(KSysGuard ${KF6_MIN_VERSION} REQUIRED)
# Pre-staged for Phase 6 (Plasma::Svg -> KSvg QML/C++ port).
find_package(KF6Svg ${KF6_MIN_VERSION} REQUIRED)
find_package(KWayland REQUIRED)
# Pre-staged for Phase 4 (dock anchoring via layer-shell).
find_package(LayerShellQt REQUIRED)

# Plasma_VERSION (libplasma) stands in for the old LibTaskManager_VERSION; same workspace release.
add_definitions(-DPLASMA_WORKSPACE_VERSION="${Plasma_VERSION}")


find_package(QtWaylandScanner REQUIRED)
find_package(PlasmaWaylandProtocols 1.6 REQUIRED)
find_package(Wayland REQUIRED COMPONENTS Client)

string(REGEX MATCH "\\.([^]]+)\\." KF6_VERSION_MINOR ${KF6Config_VERSION})
string(REGEX REPLACE "\\." "" KF6_VERSION_MINOR ${KF6_VERSION_MINOR})

message(STATUS "KF6 VERSION : ${KF6Config_VERSION}")
message(STATUS "KF6 VERSION MINOR : ${KF6_VERSION_MINOR}")


include(ECMFindQmlModule)
ecm_find_qmlmodule(QtQuick 6.6)
ecm_find_qmlmodule(QtQuick.Layouts 6.6)
# QtQuick.Controls 1.x, QtQuick.Controls.Styles.Plasma, QtGraphicalEffects are Qt5-only;
# org.kde.plasma.* QML module checks return in Phases 6-7 (QQC2 + KSvg/Kirigami port).

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/app/config-latte.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/app/config-latte.h)

#hide warnings
string(REPLACE "-Wall" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "-Wdeprecated-declarations" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "-Wreorder" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "-Wunused-variable" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "-Wunused-parameter" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

#add format security check
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat -Werror=format-security")

message(STATUS "COMPILER FLAGS : ${CMAKE_CXX_FLAGS}")

# Share Same Core Types between declarativeimports and app
set(CORETYPESHEADER "LIBCORETYPES_H")
configure_file(declarativeimports/coretypes.h.in declarativeimports/core/types.h)
set(CORETYPESHEADER "APPCORETYPES_H")
configure_file(declarativeimports/coretypes.h.in app/coretypes.h)
set(CORETYPESHEADER "LATTEGENERALCONTAINMENTTYPES_H")
configure_file(declarativeimports/coretypes.h.in containment/plugin/lattetypes.h)

# Share App::Settings::GenericTools to containment actions
configure_file(app/settings/generic/generictools.h containmentactions/contextmenu/generictools.h)
configure_file(app/settings/generic/generictools.cpp containmentactions/contextmenu/generictools.cpp)
configure_file(app/data/contextmenudata.h containmentactions/contextmenu/contextmenudata.h)

# subdirectories
add_subdirectory(declarativeimports)
add_subdirectory(indicators)
add_subdirectory(app)
add_subdirectory(containmentactions)
add_subdirectory(containment)
add_subdirectory(icons)
add_subdirectory(plasmoid)
add_subdirectory(shell)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

ki18n_install(po)
