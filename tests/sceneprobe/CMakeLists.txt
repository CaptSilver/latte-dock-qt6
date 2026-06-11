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
