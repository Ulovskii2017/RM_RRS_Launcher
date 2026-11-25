cmake_minimum_required(VERSION 3.21)

project(RM_RRS_Launcher VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use Debug by default for a fresh single-configuration CMake build.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Xml Network)
qt_standard_project_setup()

if(WIN32)
    qt_add_executable(RRSLauncher
        main.cpp
        rrs_launcher.qrc
        rrs_launcher.rc
    )
    set_target_properties(RRSLauncher PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(RRSLauncher PRIVATE dxgi Version)
else()
    qt_add_executable(RRSLauncher
        main.cpp
        rrs_launcher.qrc
    )
endif()

target_link_libraries(RRSLauncher PRIVATE Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Xml Qt6::Network)
