cmake_minimum_required(VERSION 3.16)

project(CMT VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 在这里加上 SerialPort
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(CMT
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        serialportcombobox.h
        serialportcombobox.cpp


    )
else()
    if(ANDROID)
        add_library(CMT SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(CMT
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# 2. 在这里加上 SerialPort 链接库
target_link_libraries(CMT PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
)

# 3. 把源码目录加进头文件搜索路径
#    原因：控件被"提升"后，uic 生成的 ui_mainwindow.h（在 build 目录里）
#    会写 #include "serialportcombobox.h"，必须能找到源码目录下的这个文件
target_include_directories(CMT PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.CMT)
endif()
set_target_properties(CMT PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS CMT
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(CMT)
endif()