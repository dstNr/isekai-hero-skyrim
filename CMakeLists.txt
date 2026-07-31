cmake_minimum_required(VERSION 3.21)

project(
    IsekaiHeroSKSE
    VERSION 0.6.1
    LANGUAGES CXX
)

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    AUTHOR "dstNr"
    SOURCES
        src/main.cpp
        src/Config.cpp
        src/Config.h
        src/System.cpp
        src/System.h
        src/Passives.cpp
        src/Passives.h
        src/Plugin.cpp
        src/Plugin.h
        src/Progression.cpp
        src/Progression.h
        src/SkillTree.cpp
        src/SkillTree.h
        src/SkyrimNet.cpp
        src/SkyrimNet.h
        src/Sounds.cpp
        src/Sounds.h
        src/Storage.cpp
        src/Storage.h
        src/CraftHooks.cpp
        src/CraftHooks.h
        src/PrismaUI_API.h
        src/Logger.h
        src/PCH.h
        src/UI/Input.cpp
        src/UI/Input.h
        src/UI/LevelUpEffect.cpp
        src/UI/LevelUpEffect.h
        src/UI/Overlay.cpp
        src/UI/Overlay.h
        src/UI/Prisma.cpp
        src/UI/Prisma.h
        src/UI/SkillTreeWindow.cpp
        src/UI/SkillTreeWindow.h
        src/UI/Style.h
        src/UI/SystemWindow.cpp
        src/UI/SystemWindow.h
        src/UI/Textures.cpp
        src/UI/Textures.h
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# ImGui renders straight into the game's D3D11 swap chain (see src/UI/Overlay.cpp).
# MinHook detours the crafting inventory-count functions (see src/CraftHooks.cpp).
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui d3d11.lib dxgi.lib minhook::minhook)

# stb_image decodes the panel icon PNGs (header-only; vcpkg ships no CMake config).
find_path(STB_INCLUDE_DIRS "stb_image.h" REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${STB_INCLUDE_DIRS})

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
    # Privacy: by default the linker writes the PDB's FULL build path into the DLL's
    # debug directory — here that path starts with the developer's user folder
    # (C:\Users\<real name>\...), which then ships inside every released DLL.
    # %_PDB% expands to the bare filename, so the DLL references "IsekaiHeroSKSE.pdb"
    # and no absolute path (nor the name in it) is embedded. The .pdb file itself
    # still holds full source paths, so it is deliberately NOT shipped (see package.ps1).
    target_link_options(${PROJECT_NAME} PRIVATE /PDBALTPATH:%_PDB%)
endif()
