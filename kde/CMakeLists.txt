cmake_minimum_required(VERSION 3.16)
project(bendwall LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Quick Qml Gui)

add_library(bendwallplugin SHARED frameitem.cpp plugin.cpp)
target_link_libraries(bendwallplugin PRIVATE Qt6::Quick Qt6::Qml Qt6::Gui)
set_target_properties(bendwallplugin PROPERTIES
    OUTPUT_NAME bendwallplugin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/com/philip/bendwall)

add_executable(bendwall-preview preview.cpp props.cpp)
target_link_libraries(bendwall-preview PRIVATE Qt6::Quick Qt6::Qml Qt6::Gui)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/module/qmldir
     DESTINATION ${CMAKE_BINARY_DIR}/com/philip/bendwall)
