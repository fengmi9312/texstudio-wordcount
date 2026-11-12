cmake_minimum_required(VERSION 3.16)
project(texstudio_wordcount_tests LANGUAGES CXX)

enable_testing()

add_executable(latexwordcounter_test
    latexwordcounter_test.cpp
    ../src/latexwordcounter.cpp
)
target_include_directories(latexwordcounter_test PRIVATE ../src)
set_target_properties(latexwordcounter_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
add_test(NAME latexwordcounter COMMAND latexwordcounter_test)
