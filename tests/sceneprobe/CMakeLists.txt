find_package(Vulkan REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)

option(LATTE_SCENEPROBE_ASAN "Build latte-sceneprobe with AddressSanitizer" OFF)

add_executable(latte-sceneprobe main.cpp)
target_link_libraries(latte-sceneprobe
    Qt6::Gui Qt6::Quick Qt6::Qml Qt6::GuiPrivate Vulkan::Vulkan)

if(LATTE_SCENEPROBE_ASAN)
    target_compile_options(latte-sceneprobe PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(latte-sceneprobe PRIVATE -fsanitize=address)
endif()

add_library(latte-imagecompare STATIC imagecompare.cpp)
target_link_libraries(latte-imagecompare PUBLIC Qt6::Gui)

include(ECMAddTests)
ecm_add_test(imagecompare_test.cpp
    TEST_NAME sceneprobe_imagecompare
    LINK_LIBRARIES latte-imagecompare Qt6::Test)

add_executable(latte-imgdiff imgdiff_main.cpp)
target_link_libraries(latte-imgdiff Qt6::Gui latte-imagecompare)
