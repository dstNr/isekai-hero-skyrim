# Unit tests for the engine-free parts of the plugin. A project of its own, not a
# target of the plugin build: that one needs vcpkg and CommonLibSSE, these need
# nothing but a C++ compiler. Run with tools/run-unit-tests.ps1.
cmake_minimum_required(VERSION 3.21)
project(IsekaiHeroTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

foreach(name enchant_logic readiness selftest_format tick_throttle ui_logic)
    add_executable(${name}_tests ${name}_tests.cpp)
    if(MSVC)
        target_compile_options(${name}_tests PRIVATE /W4 /WX /utf-8 /permissive-)
    endif()
    add_test(NAME ${name} COMMAND ${name}_tests)
endforeach()
