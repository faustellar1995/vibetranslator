cmake_minimum_required(VERSION 3.16)

project(qt_translator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 优先 Qt6，找不到再退回 Qt5(>=5.12)
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
    set(QT_LIBS Qt6::Widgets)
else()
    find_package(Qt5 5.12 COMPONENTS Widgets REQUIRED)
    set(QT_LIBS Qt5::Widgets)
endif()

set(APP_SOURCES
    config.h config.cpp
    deepseek.h deepseek.cpp
    translatebubble.h translatebubble.cpp
    translator.h translator.cpp
    mainwindow.h mainwindow.cpp
)

add_executable(qt_translator WIN32 main.cpp ${APP_SOURCES})
target_link_libraries(qt_translator PRIVATE ${QT_LIBS} winhttp)
target_include_directories(qt_translator PRIVATE 3rdparty/uia)
target_compile_definitions(qt_translator PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(qt_translator PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)

# 控制台测试程序（同一套源码）
add_executable(qt_translator_tests tests.cpp ${APP_SOURCES})
target_link_libraries(qt_translator_tests PRIVATE ${QT_LIBS} winhttp)
target_include_directories(qt_translator_tests PRIVATE 3rdparty/uia)
target_compile_definitions(qt_translator_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(qt_translator_tests PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
