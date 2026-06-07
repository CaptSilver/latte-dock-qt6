include(ECMAddTests)

ecm_add_test(importernametest.cpp
    TEST_NAME importernametest
    LINK_LIBRARIES Qt6::Test Qt6::Core
)

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

add_subdirectory(manual)
