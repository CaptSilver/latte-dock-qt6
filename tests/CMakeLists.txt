include(ECMAddTests)

ecm_add_test(importernametest.cpp
    TEST_NAME importernametest
    LINK_LIBRARIES Qt6::Test Qt6::Core KF6::ConfigCore
)

ecm_add_test(indicatorsknsrctest.cpp
    TEST_NAME indicatorsknsrctest
    LINK_LIBRARIES Qt6::Test Qt6::Core KF6::ConfigCore
)

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

ecm_add_test(activitiesinfotest.cpp ../app/data/activitiesinfo.cpp
    TEST_NAME activitiesinfotest
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::DBus
)
target_include_directories(activitiesinfotest PRIVATE ${CMAKE_SOURCE_DIR}/app/data)

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

ecm_add_test(visibilityrevealtest.cpp
    TEST_NAME visibilityrevealtest
    LINK_LIBRARIES Qt6::Test Qt6::Gui Plasma::Plasma Plasma::PlasmaQuick
)
target_include_directories(visibilityrevealtest PRIVATE ${CMAKE_BINARY_DIR}/app)

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

# When coverage is on, instrument every test binary AND the production sources
# linked into it (ecm_add_test compiles the listed app/* sources into the target).
# Add new test names here when adding tests.
if(LATTE_COVERAGE)
    set(_latte_cov_targets
        importernametest indicatorsknsrctest templatesnametest storageroundtriptest
        abstractlayouttest bindingrestoremodetest alternativescreateapplettest
        activitiesinfotest syncedlaunchersclienttest layershellmappingtest
        windowinfowraptest visibilityrevealtest coretypesenumtest appletzoomsizetest
        panelbackgroundtest configcontrolstest contextmenudefaulttest
        editmodehandleactiontest smartlauncheritemtest tasksbackendtest)
    foreach(_t ${_latte_cov_targets})
        target_compile_options(${_t} PRIVATE ${LATTE_COV_COMPILE_FLAGS})
        target_link_options(${_t} PRIVATE ${LATTE_COV_LINK_FLAGS})
    endforeach()
endif()

add_subdirectory(manual)
