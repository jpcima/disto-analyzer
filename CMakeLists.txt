cmake_minimum_required(VERSION "3.7")

project(disto-analyzer)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(PkgConfig REQUIRED)
find_package(Qt5 COMPONENTS Widgets DataVisualization REQUIRED)
find_package(Qwt REQUIRED)
pkg_check_modules(Jack "jack" REQUIRED IMPORTED_TARGET)

add_executable(disto-analyzer "main.cpp" "main_window.ui")
target_include_directories(disto-analyzer PRIVATE ${QWT_INCLUDE_DIRS})
target_link_libraries(disto-analyzer PRIVATE Qt5::Widgets PkgConfig::Jack ${QWT_LIBRARIES})
