include(ECMAddTests)

ecm_add_test(indicatorsknsrctest.cpp
    TEST_NAME indicatorsknsrctest
    LINK_LIBRARIES Qt6::Test Qt6::Core KF6::ConfigCore
)

ecm_add_test(indicatormetadatatest.cpp
    TEST_NAME indicatormetadatatest
    LINK_LIBRARIES Qt6::Test Qt6::Core KF6::CoreAddons
)
target_compile_definitions(indicatormetadatatest PRIVATE
    REPO_ROOT="${CMAKE_SOURCE_DIR}")

ecm_add_test(templatesnametest.cpp
    TEST_NAME templatesnametest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)

ecm_add_test(storageroundtriptest.cpp
    TEST_NAME storageroundtriptest
    LINK_LIBRARIES Qt6::Test Qt6::Core KF6::ConfigCore
)
target_compile_definitions(storageroundtriptest PRIVATE
    DOCK_TEMPLATE_PATH="${CMAKE_SOURCE_DIR}/shell/package/contents/templates/Default Dock.view.latte")

ecm_add_test(abstractlayouttest.cpp ${CMAKE_SOURCE_DIR}/app/layout/abstractlayout.cpp
    TEST_NAME abstractlayouttest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui KF6::ConfigCore Plasma::Plasma
)
target_include_directories(abstractlayouttest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/layout ${CMAKE_BINARY_DIR}/app)
target_compile_definitions(indicatorsknsrctest PRIVATE
    INDICATORS_KNSRC_PATH="${CMAKE_SOURCE_DIR}/app/latte-indicators.knsrc")

ecm_add_test(datatypestest.cpp
    ${CMAKE_SOURCE_DIR}/app/data/genericdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/activitydata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/appletdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutcolordata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layouticondata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/errorinformationdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/errordata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/generictable.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/viewdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/screendata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/viewstable.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutstable.cpp
    ${CMAKE_SOURCE_DIR}/app/tools/commontools.cpp
    TEST_NAME datatypestest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui KF6::ConfigCore Plasma::Plasma
)
target_include_directories(datatypestest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/data ${CMAKE_BINARY_DIR}/app)

ecm_add_test(settingsmodelstest.cpp
    ${CMAKE_SOURCE_DIR}/app/settings/exporttemplatedialog/appletsmodel.cpp
    ${CMAKE_SOURCE_DIR}/app/settings/screensdialog/screensmodel.cpp
    ${CMAKE_SOURCE_DIR}/app/data/genericdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/activitydata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/appletdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutcolordata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layouticondata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/errorinformationdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/errordata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/generictable.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/viewdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/screendata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/viewstable.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutstable.cpp
    ${CMAKE_SOURCE_DIR}/app/tools/commontools.cpp
    TEST_NAME settingsmodelstest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui KF6::ConfigCore KF6::I18n Plasma::Plasma
)
target_include_directories(settingsmodelstest PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/app/data
    ${CMAKE_SOURCE_DIR}/app/settings/exporttemplatedialog
    ${CMAKE_SOURCE_DIR}/app/settings/screensdialog
    ${CMAKE_BINARY_DIR}/app)

ecm_add_test(commontoolstest.cpp ${CMAKE_SOURCE_DIR}/app/tools/commontools.cpp
    TEST_NAME commontoolstest
    LINK_LIBRARIES Qt6::Test Qt6::Gui
)
target_include_directories(commontoolstest PRIVATE ${CMAKE_SOURCE_DIR}/app)

# screengeometrycalculator.cpp is the pure available-screen geometry math lifted
# out of Latte::Corona. Its header reaches only Qt + Plasma::Types enums, so the
# real source links directly with no View/Corona graph.
ecm_add_test(screengeometrycalculatortest.cpp ${CMAKE_SOURCE_DIR}/app/screengeometrycalculator.cpp
    TEST_NAME screengeometrycalculatortest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Plasma::Plasma
)
target_include_directories(screengeometrycalculatortest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_BINARY_DIR}/app)

# storageidremapper.cpp is the pure id-assignment algorithm lifted out of
# Storage::newUniqueIdsFile. Header reaches only Qt, so the real source links
# directly with no KConfig/Corona graph.
ecm_add_test(storageidremappertest.cpp ${CMAKE_SOURCE_DIR}/app/layouts/storageidremapper.cpp
    TEST_NAME storageidremappertest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)
target_include_directories(storageidremappertest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/layouts ${CMAKE_BINARY_DIR}/app)
set_tests_properties(storageidremappertest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# coronahelpers.cpp is pure string/config logic lifted out of Latte::Corona
# (layout-file path checks + obsolete-config pruning). Header reaches only Qt +
# KConfigCore, so the real source links directly with no View/Corona graph.
ecm_add_test(coronahelperstest.cpp ${CMAKE_SOURCE_DIR}/app/coronahelpers.cpp
    TEST_NAME coronahelperstest
    LINK_LIBRARIES Qt6::Test Qt6::Core KF6::ConfigCore
)
target_include_directories(coronahelperstest PRIVATE ${CMAKE_SOURCE_DIR}/app)

# startuplayoutplanner.cpp is the pure startup layout-selection decision lifted out of
# Latte::Corona::load(). It depends only on the MemoryUsage enum in apptypes.h, so the
# real source links directly with no Corona graph.
ecm_add_test(startuplayoutplannertest.cpp ${CMAKE_SOURCE_DIR}/app/startuplayoutplanner.cpp
    TEST_NAME startuplayoutplannertest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)
target_include_directories(startuplayoutplannertest PRIVATE ${CMAKE_SOURCE_DIR}/app)
set_tests_properties(startuplayoutplannertest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# generictools.cpp is a bag of Settings::Generic free helpers (QStyleOption/QPainter
# painters + string/format/geometry helpers). Its header pulls in only Qt, so the
# real source links directly. It calls QApplication::style()/QStyle, so it needs
# Qt6::Widgets and a QApplication (QTEST_MAIN, offscreen QPA).
ecm_add_test(generictoolstest.cpp ${CMAKE_SOURCE_DIR}/app/settings/generic/generictools.cpp
    TEST_NAME generictoolstest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
)
target_include_directories(generictoolstest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/settings/generic)
set_tests_properties(generictoolstest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(coretoolstest.cpp
    ${CMAKE_SOURCE_DIR}/declarativeimports/core/tools.cpp
    ${CMAKE_SOURCE_DIR}/declarativeimports/core/environment.cpp
    ${CMAKE_SOURCE_DIR}/declarativeimports/core/extras.cpp
    TEST_NAME coretoolstest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Plasma::Plasma
)
target_include_directories(coretoolstest PRIVATE
    ${CMAKE_SOURCE_DIR}/declarativeimports/core
    ${CMAKE_BINARY_DIR}/declarativeimports/core)

ecm_add_test(bindingrestoremodetest.cpp
    TEST_NAME bindingrestoremodetest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)
target_compile_definitions(bindingrestoremodetest PRIVATE
    REPO_ROOT="${CMAKE_SOURCE_DIR}")

ecm_add_test(alternativescreateapplettest.cpp
    TEST_NAME alternativescreateapplettest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)

ecm_add_test(sourceguardtest.cpp
    TEST_NAME sourceguardtest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)
target_compile_definitions(sourceguardtest PRIVATE
    REPO_ROOT="${CMAKE_SOURCE_DIR}")

ecm_add_test(activitiesinfotest.cpp ../app/data/activitiesinfo.cpp
    TEST_NAME activitiesinfotest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::DBus
)
target_include_directories(activitiesinfotest PRIVATE ${CMAKE_SOURCE_DIR}/app/data)

ecm_add_test(activitystatescachetest.cpp ../app/data/activitiesinfo.cpp
    TEST_NAME activitystatescachetest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::DBus
)
target_include_directories(activitystatescachetest PRIVATE ${CMAKE_SOURCE_DIR}/app/data)

ecm_add_test(syncedlaunchersclienttest.cpp
    TEST_NAME syncedlaunchersclienttest
    LINK_LIBRARIES Qt6::Test Qt6::Gui Qt6::Quick
)
set_tests_properties(syncedlaunchersclienttest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(layershellmappingtest.cpp ../app/wm/waylandlayershell.cpp
    TEST_NAME layershellmappingtest
    LINK_LIBRARIES Qt6::Test Qt6::Gui LayerShellQt::Interface Plasma::Plasma
)
target_include_directories(layershellmappingtest PRIVATE ${CMAKE_BINARY_DIR}/app)

ecm_add_test(windowinfowraptest.cpp ../app/wm/windowinfowrap.cpp
    TEST_NAME windowinfowraptest
    LINK_LIBRARIES Qt6::Test Qt6::Gui Plasma::Plasma
)
target_include_directories(windowinfowraptest PRIVATE ${CMAKE_BINARY_DIR}/app)

ecm_add_test(windowindextest.cpp ../app/wm/windowinfowrap.cpp
    TEST_NAME windowindextest
    LINK_LIBRARIES Qt6::Test Qt6::Gui Plasma::Plasma
)
target_include_directories(windowindextest PRIVATE ${CMAKE_BINARY_DIR}/app)

ecm_add_test(windowtrackingpredicatestest.cpp ../app/wm/windowinfowrap.cpp
    TEST_NAME windowtrackingpredicatestest
    LINK_LIBRARIES Qt6::Test Qt6::Gui Plasma::Plasma)
target_include_directories(windowtrackingpredicatestest PRIVATE ${CMAKE_SOURCE_DIR}/app ${CMAKE_BINARY_DIR}/app)
set_tests_properties(windowtrackingpredicatestest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(visibilityrevealtest.cpp
    TEST_NAME visibilityrevealtest
    LINK_LIBRARIES Qt6::Test Qt6::Gui Plasma::Plasma Plasma::PlasmaQuick
)
target_include_directories(visibilityrevealtest PRIVATE ${CMAKE_BINARY_DIR}/app)

# tasksmodel.cpp and indicator/indicatorinfo.cpp are both self-contained
# QObject/model classes (no Corona/View graph), so link them directly.
ecm_add_test(viewmodelstest.cpp
    ${CMAKE_SOURCE_DIR}/app/view/tasksmodel.cpp
    ${CMAKE_SOURCE_DIR}/app/view/indicator/indicatorinfo.cpp
    TEST_NAME viewmodelstest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
        Plasma::Plasma Plasma::PlasmaQuick KF6::Svg
)
target_include_directories(viewmodelstest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/view ${CMAKE_BINARY_DIR}/app)
set_tests_properties(viewmodelstest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

set_source_files_properties(
    ${CMAKE_BINARY_DIR}/app/qwayland-kde-primary-output-v1.cpp
    ${CMAKE_BINARY_DIR}/app/wayland-kde-primary-output-v1-protocol.c
    PROPERTIES GENERATED TRUE)
# Both ScreenPool implementations are self-contained: their headers pull in only
# data/screendata.h and primaryoutputwatcher.h, not the Corona/View graph, so the
# real production sources link directly. The generated qwayland header lives in
# ${CMAKE_BINARY_DIR}/app, so that build dir must dependency-build first.
ecm_add_test(screenpooltest.cpp
    ${CMAKE_SOURCE_DIR}/app/screenpool.cpp
    ${CMAKE_SOURCE_DIR}/app/plasma/extended/screenpool.cpp
    ${CMAKE_SOURCE_DIR}/app/primaryoutputwatcher.cpp
    ${CMAKE_BINARY_DIR}/app/qwayland-kde-primary-output-v1.cpp
    ${CMAKE_BINARY_DIR}/app/wayland-kde-primary-output-v1-protocol.c
    ${CMAKE_SOURCE_DIR}/app/data/genericdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/activitydata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/appletdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutcolordata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layouticondata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/errorinformationdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/errordata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/generictable.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/viewdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/screendata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/viewstable.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutstable.cpp
    ${CMAKE_SOURCE_DIR}/app/tools/commontools.cpp
    TEST_NAME screenpooltest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::WaylandClient
        KF6::ConfigCore KF6::CoreAddons KF6::I18n KF6::WindowSystem
        Plasma::Plasma Plasma::KWaylandClient Wayland::Client
)
add_dependencies(screenpooltest latte-dock)
target_include_directories(screenpooltest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/data ${CMAKE_BINARY_DIR}/app)
set_tests_properties(screenpooltest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_wrap_cpp(coretypesenumtest_MOC_SRCS ${CMAKE_BINARY_DIR}/app/coretypes.h)
ecm_add_test(coretypesenumtest.cpp ${coretypesenumtest_MOC_SRCS}
    TEST_NAME coretypesenumtest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)
target_include_directories(coretypesenumtest PRIVATE ${CMAKE_BINARY_DIR}/app)

ecm_add_test(appletzoomsizetest.cpp
    TEST_NAME appletzoomsizetest
    LINK_LIBRARIES Qt6::Test Qt6::Qml Qt6::Gui
)

ecm_add_test(panelbackgroundtest.cpp
    TEST_NAME panelbackgroundtest
    LINK_LIBRARIES Qt6::Test Qt6::Qml Qt6::Gui
)
target_compile_definitions(panelbackgroundtest PRIVATE
    PANEL_QML_PATH="${CMAKE_SOURCE_DIR}/shell/package/contents/views/Panel.qml")

# panelbackgroundscan.cpp is the pure pixel-loop math extracted from PanelBackground.
# Its header reaches only Qt, so the real source links directly with no KSvg/Plasma graph.
ecm_add_test(panelbackgroundscantest.cpp ${CMAKE_SOURCE_DIR}/app/plasma/extended/panelbackgroundscan.cpp
    TEST_NAME panelbackgroundscantest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui)
target_include_directories(panelbackgroundscantest PRIVATE ${CMAKE_SOURCE_DIR}/app/plasma/extended)

ecm_add_test(configcontrolstest.cpp
    TEST_NAME configcontrolstest
    LINK_LIBRARIES Qt6::Test Qt6::Qml Qt6::Gui
)
target_compile_definitions(configcontrolstest PRIVATE
    CHECKBOX_QML_PATH="${CMAKE_SOURCE_DIR}/declarativeimports/components/CheckBox.qml"
    TEXTFIELD_QML_PATH="${CMAKE_SOURCE_DIR}/declarativeimports/components/TextField.qml"
    ITEMDELEGATE_QML_PATH="${CMAKE_SOURCE_DIR}/declarativeimports/components/ItemDelegate.qml")

ecm_add_test(contextmenudefaulttest.cpp
    TEST_NAME contextmenudefaulttest
    LINK_LIBRARIES Qt6::Test Qt6::Gui Plasma::Plasma
)
target_compile_definitions(contextmenudefaulttest PRIVATE
    LATTE_DEFAULTS_PATH="${CMAKE_SOURCE_DIR}/shell/package/contents/defaults"
    VIEW_CPP_PATH="${CMAKE_SOURCE_DIR}/app/view/view.cpp"
    LATTE_CONTEXTMENU_PLUGIN="${CMAKE_BINARY_DIR}/bin/org.kde.latte.contextmenu.so")

ecm_add_test(editmodehandleactiontest.cpp
    TEST_NAME editmodehandleactiontest
    LINK_LIBRARIES Qt6::Test Qt6::Gui
)
target_compile_definitions(editmodehandleactiontest PRIVATE
    CONFIGOVERLAY_QML_PATH="${CMAKE_SOURCE_DIR}/containment/package/contents/ui/editmode/ConfigOverlay.qml")

qt_wrap_cpp(appletremovaltest_TYPES_MOC ${CMAKE_BINARY_DIR}/containment/plugin/lattetypes.h)
ecm_add_test(appletremovaltest.cpp
    ${CMAKE_SOURCE_DIR}/containment/plugin/layoutmanager.cpp
    ${appletremovaltest_TYPES_MOC}
    TEST_NAME appletremovaltest
    LINK_LIBRARIES Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Plasma::Plasma Plasma::PlasmaQuick KF6::ConfigQml
)
target_include_directories(appletremovaltest PRIVATE
    ${CMAKE_SOURCE_DIR}/containment/plugin
    ${CMAKE_BINARY_DIR}/containment)
set_tests_properties(appletremovaltest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_wrap_cpp(layoutmanagertest_TYPES_MOC ${CMAKE_BINARY_DIR}/containment/plugin/lattetypes.h)
ecm_add_test(layoutmanagertest.cpp
    ${CMAKE_SOURCE_DIR}/containment/plugin/layoutmanager.cpp
    ${layoutmanagertest_TYPES_MOC}
    TEST_NAME layoutmanagertest
    LINK_LIBRARIES Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Plasma::Plasma Plasma::PlasmaQuick KF6::ConfigQml
)
target_include_directories(layoutmanagertest PRIVATE
    ${CMAKE_SOURCE_DIR}/containment/plugin
    ${CMAKE_BINARY_DIR}/containment)
set_tests_properties(layoutmanagertest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(viewprioritytest.cpp
    ${CMAKE_SOURCE_DIR}/app/layout/viewpriority.cpp
    ${CMAKE_SOURCE_DIR}/app/data/viewdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/genericdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/activitydata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/appletdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutcolordata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layouticondata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/errorinformationdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/errordata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/generictable.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutdata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/screendata.cpp
    ${CMAKE_SOURCE_DIR}/app/data/viewstable.cpp
    ${CMAKE_SOURCE_DIR}/app/data/layoutstable.cpp
    ${CMAKE_SOURCE_DIR}/app/tools/commontools.cpp
    TEST_NAME viewprioritytest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui KF6::ConfigCore Plasma::Plasma
)
target_include_directories(viewprioritytest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/data ${CMAKE_BINARY_DIR}/app)
set_tests_properties(viewprioritytest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(validviewsmapbuildertest.cpp
    ${CMAKE_SOURCE_DIR}/app/layout/validviewsmapbuilder.cpp
    TEST_NAME validviewsmapbuildertest
    LINK_LIBRARIES Qt6::Test Qt6::Core Plasma::Plasma
)
target_include_directories(validviewsmapbuildertest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_BINARY_DIR}/app)
set_tests_properties(validviewsmapbuildertest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(viewsyncplantest.cpp ${CMAKE_SOURCE_DIR}/app/layout/viewsyncplan.cpp
    TEST_NAME viewsyncplantest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)
target_include_directories(viewsyncplantest PRIVATE ${CMAKE_SOURCE_DIR}/app ${CMAKE_BINARY_DIR}/app)
set_tests_properties(viewsyncplantest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(addviewdecisiontest.cpp ${CMAKE_SOURCE_DIR}/app/layout/addviewdecision.cpp
    TEST_NAME addviewdecisiontest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)
target_include_directories(addviewdecisiontest PRIVATE ${CMAKE_SOURCE_DIR}/app ${CMAKE_BINARY_DIR}/app)
set_tests_properties(addviewdecisiontest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(viewcontainertransitiontest.cpp
    TEST_NAME viewcontainertransitiontest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)
target_include_directories(viewcontainertransitiontest PRIVATE ${CMAKE_SOURCE_DIR}/app)
set_tests_properties(viewcontainertransitiontest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(activitysetalgebratest.cpp
    TEST_NAME activitysetalgebratest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)
target_include_directories(activitysetalgebratest PRIVATE ${CMAKE_SOURCE_DIR}/app ${CMAKE_BINARY_DIR}/app)
set_tests_properties(activitysetalgebratest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(settingsnameutilstest.cpp
    TEST_NAME settingsnameutilstest
    LINK_LIBRARIES Qt6::Test Qt6::Core)
target_include_directories(settingsnameutilstest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/settings ${CMAKE_BINARY_DIR}/app)
set_tests_properties(settingsnameutilstest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(iconsourceclassifiertest.cpp
    TEST_NAME iconsourceclassifiertest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui)
target_include_directories(iconsourceclassifiertest PRIVATE ${CMAKE_SOURCE_DIR}/declarativeimports/core)
set_tests_properties(iconsourceclassifiertest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(coverage/selftest/covselftest.cpp coverage/selftest/covself.cpp
    TEST_NAME covselftest
    LINK_LIBRARIES Qt6::Test
)

ecm_add_test(shortcutstest.cpp
    ${CMAKE_SOURCE_DIR}/app/shortcuts/shortcutstracker.cpp
    ${CMAKE_SOURCE_DIR}/app/shortcuts/modifiertracker.cpp
    ${CMAKE_SOURCE_DIR}/app/tools/commontools.cpp
    TEST_NAME shortcutstest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui
        KF6::ConfigCore KF6::CoreAddons KF6::GuiAddons KF6::GlobalAccel
)
target_include_directories(shortcutstest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/shortcuts ${CMAKE_BINARY_DIR}/app)
set_tests_properties(shortcutstest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# UniversalSettings only casts its parent to Latte::Corona (null here) but that
# qobject_cast odr-uses Corona's metaobject, which drags in the whole application
# object graph. Rather than recompile half the app, link the real production
# objects the latte-dock executable already built (everything except main.cpp),
# and drive them through the header. This is a genuine real-link against the
# shipped universalsettings.cpp.o.
set(_latte_app_objdir ${CMAKE_BINARY_DIR}/app/CMakeFiles/latte-dock.dir)
file(GLOB_RECURSE _latte_app_objs
    "${_latte_app_objdir}/*.cpp.o"
    "${_latte_app_objdir}/*.c.o")
list(REMOVE_ITEM _latte_app_objs "${_latte_app_objdir}/main.cpp.o")

ecm_add_test(universalsettingstest.cpp
    TEST_NAME universalsettingstest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
        Plasma::Activities KF6::Archive KF6::CoreAddons KF6::Crash KF6::DBusAddons
        KF6::ConfigQml KF6::ConfigCore KF6::GuiAddons KF6::GlobalAccel KF6::I18n
        KF6::IconThemes KF6::IconWidgets KF6::KIOWidgets KF6::Notifications
        KF6::NewStuffWidgets Plasma::Plasma KF6::Svg KF6::Package Plasma::PlasmaQuick
        Plasma::KWaylandClient LayerShellQt::Interface KF6::XmlGui KF6::WindowSystem
        Wayland::Client
)
# Pull in the prebuilt application objects (minus main) as a link dependency so the
# test rebuilds whenever they change, and so the test only links after the app does.
target_link_libraries(universalsettingstest ${_latte_app_objs})
add_dependencies(universalsettingstest latte-dock)
target_include_directories(universalsettingstest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/settings ${CMAKE_BINARY_DIR}/app)
set_tests_properties(universalsettingstest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# CoronaEngine drags in the whole application object graph, so it links the same
# prebuilt objects (minus main.cpp) and builds a real engine headlessly to drive
# its available-screen geometry against a fake IScreenInfo.
ecm_add_test(coronaenginegeometrytest.cpp
    TEST_NAME coronaenginegeometrytest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
        Plasma::Activities KF6::Archive KF6::CoreAddons KF6::Crash KF6::DBusAddons
        KF6::ConfigQml KF6::ConfigCore KF6::GuiAddons KF6::GlobalAccel KF6::I18n
        KF6::IconThemes KF6::IconWidgets KF6::KIOWidgets KF6::Notifications
        KF6::NewStuffWidgets Plasma::Plasma KF6::Svg KF6::Package Plasma::PlasmaQuick
        Plasma::KWaylandClient LayerShellQt::Interface KF6::XmlGui KF6::WindowSystem
        Wayland::Client
)
target_link_libraries(coronaenginegeometrytest ${_latte_app_objs})
add_dependencies(coronaenginegeometrytest latte-dock)
target_include_directories(coronaenginegeometrytest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_BINARY_DIR}/app)
set_tests_properties(coronaenginegeometrytest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# storage.cpp pulls in Corona/View/Importer/Manager through its includes, so the
# same prebuilt-objects trick as universalsettingstest links the real compiled
# Storage::self() (minus main.cpp) and drives its file/KConfigGroup methods.
ecm_add_test(storagetest.cpp
    TEST_NAME storagetest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
        Plasma::Activities KF6::Archive KF6::CoreAddons KF6::Crash KF6::DBusAddons
        KF6::ConfigQml KF6::ConfigCore KF6::GuiAddons KF6::GlobalAccel KF6::I18n
        KF6::IconThemes KF6::IconWidgets KF6::KIOWidgets KF6::Notifications
        KF6::NewStuffWidgets Plasma::Plasma KF6::Svg KF6::Package Plasma::PlasmaQuick
        Plasma::KWaylandClient LayerShellQt::Interface KF6::XmlGui KF6::WindowSystem
        Wayland::Client
)
target_link_libraries(storagetest ${_latte_app_objs})
add_dependencies(storagetest latte-dock)
target_include_directories(storagetest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/layouts ${CMAKE_BINARY_DIR}/app)
set_tests_properties(storagetest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# tasktools.cpp / schemecolors.cpp drag in the whole app object graph through their
# includes (Importer -> Corona -> ...), so the prebuilt-objects trick links the real
# compiled wm/tasktools.cpp.o and wm/schemecolors.cpp.o (minus main.cpp) and drives
# them through the headers. Genuine real-link against the shipped objects.
ecm_add_test(wmtoolstest.cpp
    TEST_NAME wmtoolstest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
        Plasma::Activities KF6::Archive KF6::CoreAddons KF6::Crash KF6::DBusAddons
        KF6::ConfigQml KF6::ConfigCore KF6::GuiAddons KF6::GlobalAccel KF6::I18n
        KF6::IconThemes KF6::IconWidgets KF6::KIOWidgets KF6::Notifications
        KF6::NewStuffWidgets Plasma::Plasma KF6::Svg KF6::Package Plasma::PlasmaQuick
        Plasma::KWaylandClient LayerShellQt::Interface KF6::XmlGui KF6::WindowSystem
        KF6::Service Wayland::Client
)
target_link_libraries(wmtoolstest ${_latte_app_objs})
add_dependencies(wmtoolstest latte-dock)
target_include_directories(wmtoolstest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/wm ${CMAKE_BINARY_DIR}/app)
set_tests_properties(wmtoolstest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# schemesmodel.cpp pulls in the whole app object graph through importer.cpp ->
# lattecorona.h, so the prebuilt-objects trick links the real compiled
# settings/detailsdialog/schemesmodel.cpp.o + wm/schemecolors.cpp.o +
# layouts/importer.cpp.o (minus main.cpp) and drives them through the headers.
# Genuine real-link against the shipped objects.
ecm_add_test(schemesmodeltest.cpp
    TEST_NAME schemesmodeltest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
        Plasma::Activities KF6::Archive KF6::CoreAddons KF6::Crash KF6::DBusAddons
        KF6::ConfigQml KF6::ConfigCore KF6::GuiAddons KF6::GlobalAccel KF6::I18n
        KF6::IconThemes KF6::IconWidgets KF6::KIOWidgets KF6::Notifications
        KF6::NewStuffWidgets Plasma::Plasma KF6::Svg KF6::Package Plasma::PlasmaQuick
        Plasma::KWaylandClient LayerShellQt::Interface KF6::XmlGui KF6::WindowSystem
        KF6::Service Wayland::Client
)
target_link_libraries(schemesmodeltest ${_latte_app_objs})
add_dependencies(schemesmodeltest latte-dock)
target_include_directories(schemesmodeltest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/settings ${CMAKE_BINARY_DIR}/app)
set_tests_properties(schemesmodeltest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# importer.cpp pulls in lattecorona.h (the full app object graph) through its
# includes, so the prebuilt-objects trick links the real compiled
# layouts/importer.cpp.o (minus main.cpp) and drives its static parser/path
# helpers through the header. Genuine real-link against the shipped object.
ecm_add_test(importerlogictest.cpp
    TEST_NAME importerlogictest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
        Plasma::Activities KF6::Archive KF6::CoreAddons KF6::Crash KF6::DBusAddons
        KF6::ConfigQml KF6::ConfigCore KF6::GuiAddons KF6::GlobalAccel KF6::I18n
        KF6::IconThemes KF6::IconWidgets KF6::KIOWidgets KF6::Notifications
        KF6::NewStuffWidgets Plasma::Plasma KF6::Svg KF6::Package Plasma::PlasmaQuick
        Plasma::KWaylandClient LayerShellQt::Interface KF6::XmlGui KF6::WindowSystem
        KF6::Service Wayland::Client
)
target_link_libraries(importerlogictest ${_latte_app_objs})
add_dependencies(importerlogictest latte-dock)
target_include_directories(importerlogictest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/layouts ${CMAKE_BINARY_DIR}/app)
set_tests_properties(importerlogictest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# lastactivewindow.cpp's header pulls in the tracker graph and its .cpp includes
# view/view.h -> the full Corona object graph, so the prebuilt-objects trick links
# the real compiled wm/tracker/*.cpp.o (minus main.cpp) and drives the real
# LastActiveWindow through a minimal concrete AbstractWindowInterface.
ecm_add_test(lastactivewindowtest.cpp
    TEST_NAME lastactivewindowtest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
        Plasma::Activities KF6::Archive KF6::CoreAddons KF6::Crash KF6::DBusAddons
        KF6::ConfigQml KF6::ConfigCore KF6::GuiAddons KF6::GlobalAccel KF6::I18n
        KF6::IconThemes KF6::IconWidgets KF6::KIOWidgets KF6::Notifications
        KF6::NewStuffWidgets Plasma::Plasma KF6::Svg KF6::Package Plasma::PlasmaQuick
        Plasma::KWaylandClient LayerShellQt::Interface KF6::XmlGui KF6::WindowSystem
        KF6::Service Wayland::Client
)
target_link_libraries(lastactivewindowtest ${_latte_app_objs})
add_dependencies(lastactivewindowtest latte-dock)
target_include_directories(lastactivewindowtest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/wm ${CMAKE_BINARY_DIR}/app)
set_tests_properties(lastactivewindowtest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# layoutsmodel.cpp's header pulls in lattecorona.h (the full app object graph),
# so the prebuilt-objects trick links the real compiled layoutsmodel.cpp.o (minus
# main.cpp) and drives the model through its header.
ecm_add_test(layoutsmodeltest.cpp
    TEST_NAME layoutsmodeltest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
        Plasma::Activities KF6::Archive KF6::CoreAddons KF6::Crash KF6::DBusAddons
        KF6::ConfigQml KF6::ConfigCore KF6::GuiAddons KF6::GlobalAccel KF6::I18n
        KF6::IconThemes KF6::IconWidgets KF6::KIOWidgets KF6::Notifications
        KF6::NewStuffWidgets Plasma::Plasma KF6::Svg KF6::Package Plasma::PlasmaQuick
        Plasma::KWaylandClient LayerShellQt::Interface KF6::XmlGui KF6::WindowSystem
        KF6::Service Wayland::Client
)
target_link_libraries(layoutsmodeltest ${_latte_app_objs})
add_dependencies(layoutsmodeltest latte-dock)
target_include_directories(layoutsmodeltest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/settings
    ${CMAKE_SOURCE_DIR}/app/settings/settingsdialog ${CMAKE_BINARY_DIR}/app)
set_tests_properties(layoutsmodeltest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# viewsmodel.cpp's header pulls in lattecorona.h (the full app object graph), so
# the prebuilt-objects trick links the real compiled viewsmodel.cpp.o (minus
# main.cpp) and drives the model through its header.
ecm_add_test(viewsmodeltest.cpp
    TEST_NAME viewsmodeltest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
        Plasma::Activities KF6::Archive KF6::CoreAddons KF6::Crash KF6::DBusAddons
        KF6::ConfigQml KF6::ConfigCore KF6::GuiAddons KF6::GlobalAccel KF6::I18n
        KF6::IconThemes KF6::IconWidgets KF6::KIOWidgets KF6::Notifications
        KF6::NewStuffWidgets Plasma::Plasma KF6::Svg KF6::Package Plasma::PlasmaQuick
        Plasma::KWaylandClient LayerShellQt::Interface KF6::XmlGui KF6::WindowSystem
        KF6::Service Wayland::Client
)
target_link_libraries(viewsmodeltest ${_latte_app_objs})
add_dependencies(viewsmodeltest latte-dock)
target_include_directories(viewsmodeltest PRIVATE
    ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/app/settings
    ${CMAKE_SOURCE_DIR}/app/settings/viewsdialog ${CMAKE_BINARY_DIR}/app)
set_tests_properties(viewsmodeltest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(smartlauncheritemtest.cpp
    ${CMAKE_SOURCE_DIR}/plasmoid/plugin/smartlauncheritem.cpp
    ${CMAKE_SOURCE_DIR}/plasmoid/plugin/smartlauncherbackend.cpp
    TEST_NAME smartlauncheritemtest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::DBus KF6::Service KF6::ConfigCore PW::LibNotificationManager
)
target_include_directories(smartlauncheritemtest PRIVATE ${CMAKE_SOURCE_DIR}/plasmoid/plugin)

set(tasksbackendtest_SRCS tasksbackendtest.cpp ${CMAKE_SOURCE_DIR}/plasmoid/plugin/backend.cpp)
kconfig_add_kcfg_files(tasksbackendtest_SRCS ${CMAKE_SOURCE_DIR}/plasmoid/plugin/kactivitymanagerd_plugins_settings.kcfgc)
ecm_add_test(${tasksbackendtest_SRCS}
    TEST_NAME tasksbackendtest
    LINK_LIBRARIES Qt6::Test Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::DBus
        Plasma::Activities Plasma::ActivitiesStats KSysGuard::ProcessCore
        KF6::Service KF6::ConfigCore KF6::ConfigGui KF6::I18n
        KF6::KIOCore KF6::KIOGui KF6::KIOFileWidgets KF6::Notifications KF6::WindowSystem
)
target_include_directories(tasksbackendtest PRIVATE ${CMAKE_SOURCE_DIR}/plasmoid/plugin ${CMAKE_CURRENT_BINARY_DIR})

# Headless QML compile gate: compiles every shell/containment/plasmoid QML in the
# real engine (offscreen) so removed-type / removed-property errors in lazy,
# interaction-only components are caught without a live session. The script
# self-stages the packages, so it needs no prior install. It assumes the in-tree
# build environment (qmltestrunner + a writable stage dir), matching how the rest
# of the suite is built and run.
add_test(NAME qmlloadcompile COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/manual/qml_load_compile.sh)
set_tests_properties(qmlloadcompile PROPERTIES
    ENVIRONMENT "BUILD=${CMAKE_BINARY_DIR}"
    TIMEOUT 300)

# Headless interaction tests (Qt Quick Test): instantiate self-contained Latte
# controls offscreen and synthesize real clicks to assert behavior. New cases go
# in tests/qml/tst_*.qml.
add_test(NAME qmlinteraction COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/manual/qml_interaction_test.sh)
set_tests_properties(qmlinteraction PROPERTIES TIMEOUT 120)

# Static guard against the edit-mode handle flicker regressing: the ConfigOverlay handle buttons must
# never carry an attached QQC2.ToolTip (Wayland popup-surface hover-steal). No headless behavioral repro
# exists for a multi-surface compositor interaction, so this pins the fix at the source.
add_test(NAME editmodetooltipguard COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/manual/editmode_tooltip_guard.sh)
set_tests_properties(editmodetooltipguard PROPERTIES TIMEOUT 30)

# When coverage is on, instrument every test binary AND the production sources
# linked into it (ecm_add_test compiles the listed app/* sources into the target).
# Add new test names here when adding tests.
if(LATTE_COVERAGE)
    set(_latte_cov_targets
        indicatorsknsrctest templatesnametest storageroundtriptest storageidremappertest
        abstractlayouttest bindingrestoremodetest alternativescreateapplettest sourceguardtest
        activitiesinfotest activitystatescachetest syncedlaunchersclienttest layershellmappingtest
        windowinfowraptest windowindextest visibilityrevealtest coretypesenumtest appletzoomsizetest
        panelbackgroundtest panelbackgroundscantest configcontrolstest contextmenudefaulttest
        editmodehandleactiontest smartlauncheritemtest tasksbackendtest covselftest
        datatypestest commontoolstest layoutmanagertest settingsmodelstest wmtoolstest
        shortcutstest viewmodelstest screenpooltest coretoolstest generictoolstest
        viewprioritytest addviewdecisiontest viewcontainertransitiontest activitysetalgebratest
        settingsnameutilstest iconsourceclassifiertest windowtrackingpredicatestest
        # GLOB-behavioral tests link the (now instrumented) app objects; listing them
        # gives their binaries the profile link flag and the export object set, which
        # is what makes the Corona-coupled core count toward whole-app coverage.
        universalsettingstest storagetest schemesmodeltest viewsmodeltest
        layoutsmodeltest lastactivewindowtest importerlogictest)
    foreach(_t ${_latte_cov_targets})
        target_compile_options(${_t} PRIVATE ${LATTE_COV_COMPILE_FLAGS})
        target_link_options(${_t} PRIVATE ${LATTE_COV_LINK_FLAGS})
    endforeach()
    # Single source of truth for the coverage object set (read by cxx_coverage.sh),
    # so it can't drift from the actually-instrumented targets.
    string(REPLACE ";" "\n" _latte_cov_targets_lines "${_latte_cov_targets}")
    file(WRITE "${CMAKE_BINARY_DIR}/cov_targets.txt" "${_latte_cov_targets_lines}\n")
endif()

add_subdirectory(manual)
add_subdirectory(sceneprobe)
