cmake_minimum_required(VERSION 4.2)
project(qtl)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
        Core
        Network
        REQUIRED)

add_executable(qtl
    src/main.cpp
    src/httpserver.cpp
    src/logger.cpp
    src/router.cpp
)

## Include public headers in the target to ensure AUTOMOC processes Q_OBJECT
target_sources(qtl PRIVATE
        include/httpserver.h
        include/router.h
        include/logger.h
)

target_include_directories(qtl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(qtl
        Qt::Core
        Qt::Network
)

