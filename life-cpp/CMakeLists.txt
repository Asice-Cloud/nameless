cmake_minimum_required(VERSION 4.0)
project(qt_life_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable automoc so Qt's MOC runs for Q_OBJECT macros
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(qt_life_cpp
    main.cpp
    GameOfLifeWidget.cpp
    GameOfLifeWidget.h
)

target_link_libraries(qt_life_cpp PRIVATE Qt6::Widgets)
