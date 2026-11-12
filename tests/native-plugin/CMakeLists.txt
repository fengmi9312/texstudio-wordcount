cmake_minimum_required(VERSION 3.21)
project(texstudio_wordcount_native_smoke LANGUAGES CXX)

find_package(Qt6 6.11.2 EXACT REQUIRED COMPONENTS Widgets)
add_library(texstudio_wordcount_smoke MODULE native_selection_smoke.cpp)
set_target_properties(texstudio_wordcount_smoke PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    PREFIX ""
)
target_link_libraries(texstudio_wordcount_smoke PRIVATE Qt6::Widgets)
