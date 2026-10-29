cmake_minimum_required(VERSION 3.20)
project(sharkESP CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ép build x86 (32-bit)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Must build as 32-bit (x86). Configure with -A Win32.")
endif()

include(FetchContent)

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_executable(sharkESP
    main.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(sharkESP PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(sharkESP PRIVATE
    d3d11 dxgi d3dcompiler dwmapi user32 gdi32
)

set_target_properties(sharkESP PROPERTIES
    WIN32_EXECUTABLE FALSE
    OUTPUT_NAME "sharkESP"
)