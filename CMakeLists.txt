cmake_minimum_required(VERSION 3.16)

project(qt_translator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 本机默认：Qt 5.14.2 MSVC 2017 64-bit（见 local-pcl-qt / local-cpp）
if(NOT DEFINED Qt5_DIR AND EXISTS "D:/Qt/Qt5.14.2/5.14.2/msvc2017_64/lib/cmake/Qt5")
    set(Qt5_DIR "D:/Qt/Qt5.14.2/5.14.2/msvc2017_64/lib/cmake/Qt5")
endif()

find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
    set(QT_LIBS Qt6::Widgets)
else()
    find_package(Qt5 5.12 COMPONENTS Widgets REQUIRED)
    set(QT_LIBS Qt5::Widgets)
endif()

set(APP_SOURCES
    config.h config.cpp
    mimo.h mimo.cpp
    translatebubble.h translatebubble.cpp
    translator.h translator.cpp
    mainwindow.h mainwindow.cpp
)

add_executable(qt_translator WIN32 main.cpp ${APP_SOURCES})
target_link_libraries(qt_translator PRIVATE ${QT_LIBS} winhttp ole32 oleaut32 uuid)
target_compile_definitions(qt_translator PRIVATE NOMINMAX)

add_executable(qt_translator_tests tests.cpp ${APP_SOURCES})
target_link_libraries(qt_translator_tests PRIVATE ${QT_LIBS} winhttp ole32 oleaut32 uuid)
target_compile_definitions(qt_translator_tests PRIVATE NOMINMAX)

if(MSVC)
    target_compile_options(qt_translator PRIVATE /utf-8)
    target_compile_options(qt_translator_tests PRIVATE /utf-8)
endif()
