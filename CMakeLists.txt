cmake_minimum_required(VERSION 3.21)

project(
    IsekaiHeroSKSE
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(CommonLibSSE CONFIG REQUIRED)

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    AUTHOR "dstNr"
    SOURCES
        src/main.cpp
        src/System.cpp
        src/System.h
        src/Logger.h
        src/PCH.h
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# CommonLibSSE-NG expects its setup header (SKSE/Impl/PCH.h, which defines
# `namespace stl`) to be force-included before any other library header.
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

# CommonLibSSE-NG headers require MSVC's conformant mode to parse correctly.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /permissive-
        /Zc:preprocessor
    )
endif()
