cmake_minimum_required(VERSION 3.16)
project(NMD VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network Svg)
qt_standard_project_setup()

set(NMD_SOURCES
    core.cpp
    core.h
    queuedmod.h
    workflowhelpers.h
    mainwindow.h
    mainwindow.cpp
    mainwindow_ui.cpp
    mainwindow_api.cpp
    mainwindow_resolver.cpp
    mainwindow_downloads.cpp
    nmd.qrc
)

qt_add_executable(NMD WIN32 main.cpp nmd.rc ${NMD_SOURCES})
target_link_libraries(NMD PRIVATE Qt6::Widgets Qt6::Network Qt6::Svg)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(NMD PRIVATE -Wshadow=local -Werror=shadow=local)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(nmd_tests tests.cpp ${NMD_SOURCES})
    target_link_libraries(nmd_tests PRIVATE Qt6::Widgets Qt6::Network)
    add_test(NAME nmd_workflow COMMAND nmd_tests)
endif()

include(GNUInstallDirs)
install(TARGETS NMD RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
