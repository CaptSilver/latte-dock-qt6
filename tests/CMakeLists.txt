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

add_subdirectory(manual)
