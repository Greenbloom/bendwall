cmake_minimum_required(VERSION 3.16)
project(bendwall LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Quick Qml Gui OpenGL DBus)
find_package(Qt6 QUIET COMPONENTS WebEngineQuick)

add_library(bendwallplugin SHARED frameitem.cpp plugin.cpp engine.cpp catalog.cpp props.cpp)
target_link_libraries(bendwallplugin PRIVATE Qt6::Quick Qt6::Qml Qt6::Gui Qt6::OpenGL)
target_link_libraries(bendwallplugin PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(bendwallplugin PROPERTIES
    OUTPUT_NAME bendwallplugin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/com/philip/bendwall)

add_executable(bendwall-preview preview.cpp props.cpp engine.cpp)
target_link_libraries(bendwall-preview PRIVATE Qt6::Quick Qt6::Qml Qt6::Gui)
target_link_libraries(bendwall-preview PRIVATE ${CMAKE_DL_LIBS})
if (TARGET Qt6::WebEngineQuick)
    target_link_libraries(bendwall-preview PRIVATE Qt6::WebEngineQuick)
    target_compile_definitions(bendwall-preview PRIVATE BENDWALL_HAS_WEBENGINE=1)
endif()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/module/qmldir
     DESTINATION ${CMAKE_BINARY_DIR}/com/philip/bendwall)

add_executable(bendwall-picker
    picker.cpp library.cpp shellconfig.cpp
    frameitem.cpp engine.cpp props.cpp
    picker.qrc)
target_link_libraries(bendwall-picker PRIVATE Qt6::Quick Qt6::Qml Qt6::Gui Qt6::OpenGL Qt6::DBus)
target_link_libraries(bendwall-picker PRIVATE ${CMAKE_DL_LIBS})
