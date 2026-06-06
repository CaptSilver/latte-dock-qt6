add_executable(layershell_anchor_test layershell_anchor_test.cpp ../../app/wm/waylandlayershell.cpp)
target_link_libraries(layershell_anchor_test Qt6::Gui LayerShellQt::Interface Plasma::Plasma)
target_include_directories(layershell_anchor_test PRIVATE ${CMAKE_BINARY_DIR}/app)

add_executable(plasmawindow_bind_test plasmawindow_bind_test.cpp)
target_link_libraries(plasmawindow_bind_test Qt6::Gui Plasma::KWaylandClient)
