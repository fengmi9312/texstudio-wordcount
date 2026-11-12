cmake_minimum_required(VERSION 3.21)
project(texstudio_wordcount_plugin LANGUAGES CXX)

find_package(Qt6 6.11.2 EXACT REQUIRED COMPONENTS Core Gui Widgets Concurrent)
set(CMAKE_AUTOMOC ON)

add_library(texstudio-wordcount MODULE
    wordcountplugin.cpp
    ../src/latexwordcounter.cpp
)
set_target_properties(texstudio-wordcount PROPERTIES PREFIX "")
target_compile_features(texstudio-wordcount PRIVATE cxx_std_17)
target_include_directories(texstudio-wordcount PRIVATE ../src)
target_link_libraries(texstudio-wordcount PRIVATE Qt6::Widgets Qt6::Concurrent)
if(MSVC)
    target_compile_options(texstudio-wordcount PRIVATE /utf-8)
endif()
