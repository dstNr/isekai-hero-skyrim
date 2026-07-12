cmake_minimum_required(VERSION 3.21)

project(
    IsekaiHeroSKSE
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    AUTHOR "dstNr"
    SOURCES
        src/main.cpp
        src/System.cpp
        src/System.h
        src/Progression.cpp
        src/Progression.h
        src/Logger.h
        src/PCH.h
        src/UI/Input.cpp
        src/UI/Input.h
        src/UI/LevelUpEffect.cpp
        src/UI/LevelUpEffect.h
        src/UI/Overlay.cpp
        src/UI/Overlay.h
        src/UI/Style.h
        src/UI/SystemWindow.cpp
        src/UI/SystemWindow.h
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# ImGui renders straight into the game's D3D11 swap chain (see src/UI/Overlay.cpp).
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui d3d11.lib dxgi.lib)

# So sources can say #include "UI/Overlay.h".
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Pulling in d3d11.h drags in windows.h; keep its macros from colliding with
# CommonLibSSE (which deliberately avoids including it).
target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

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
