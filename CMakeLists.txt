cmake_minimum_required(VERSION 3.21)

project(
    IsekaiHeroSKSE
    VERSION 0.8.0
    LANGUAGES CXX
)

# CommonLibSSE-NG is built from source as a submodule, not from a vcpkg port. The port
# (colorglass) still ships CharmedBaryon's 3.7.0 from 2023, which cannot run Skyrim 1.7:
# it classifies the runtime by minor version (7 falls through to "SE") and its address
# library reader only knows database formats 1 and 2, while 1.7.99+ ships format 5.
# alandtse's fork is the maintained one. See extern/CommonLibSSE-NG, pinned to a tag.
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)          # Catch2, not needed to consume the library.
set(SKSE_SUPPORT_XBYAK OFF CACHE BOOL "" FORCE)   # We detour with MinHook, not Xbyak.
add_subdirectory(extern/CommonLibSSE-NG)

# Built from source, the library's CMakeLists only defines add_commonlibsse_plugin() on
# its prebuilt-bundle path, so a source consumer has to include the module itself. This
# is the pattern upstream's own reference consumer uses (test_consumer_live).
get_target_property(COMMONLIB_SRC_DIR CommonLibSSE SOURCE_DIR)
include(${COMMONLIB_SRC_DIR}/cmake/CommonLibSSE.cmake)

find_package(imgui CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)
# Required by the vendored ImGuiVRHelper client API: its InputCombo serialises through
# nlohmann_json so clients can persist bindings.
find_package(nlohmann_json CONFIG REQUIRED)

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    AUTHOR "dstNr"
    SOURCES
        src/main.cpp
        src/Config.cpp
        src/Loc.cpp
        src/Loc.h
        src/Config.h
        src/System.cpp
        src/System.h
        src/Passives.cpp
        src/Passives.h
        src/Plugin.cpp
        src/Plugin.h
        src/Professions.cpp
        src/Professions.h
        src/Progression.cpp
        src/Quests.cpp
        src/Quests.h
        src/SelfTest.cpp
        src/SelfTest.h
        src/Progression.h
        src/SkillTree.cpp
        src/SkillTree.h
        src/Shop.cpp
        src/Shop.h
        src/SkyrimNet.cpp
        src/SkyrimNet.h
        src/Sounds.cpp
        src/Sounds.h
        src/Storage.cpp
        src/Storage.h
        src/CraftHooks.cpp
        src/CraftHooks.h
        src/PrismaUI_API.h
        src/ModAPI.cpp
        src/ModAPI.h
        include/IsekaiHeroAPI.h
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
        src/UI/ShopWindow.cpp
        src/UI/ShopWindow.h
        src/UI/ThreatLabels.cpp
        src/UI/ThreatLabels.h
        src/UI/Toast.cpp
        src/UI/Toast.h
        src/UI/SkillTreeWindow.cpp
        src/UI/SkillTreeWindow.h
        src/UI/Style.h
        src/UI/SystemWindow.cpp
        src/UI/SystemWindow.h
        src/UI/Textures.cpp
        src/UI/Textures.h
        src/UI/VROverlay.cpp
        src/UI/VROverlay.h
        # Vendored, LGPL-3.0-or-later, unmodified — see extern/imgui-vr-helper/README.md.
        # Only the handshake needs compiling; everything else in there is header-only.
        extern/imgui-vr-helper/ImGuiVRHelperAPI.cpp
        extern/imgui-vr-helper/ImGuiVRHelperAPI.h
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# ImGui renders straight into the game's D3D11 swap chain (see src/UI/Overlay.cpp).
# MinHook detours the crafting inventory-count functions (see src/CraftHooks.cpp).
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui d3d11.lib dxgi.lib minhook::minhook
                                             nlohmann_json::nlohmann_json)

# stb_image decodes the panel icon PNGs (header-only; vcpkg ships no CMake config).
find_path(STB_INCLUDE_DIRS "stb_image.h" REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${STB_INCLUDE_DIRS})

# So sources can say #include "UI/Overlay.h", and #include "ImGuiVRHelperClientSDK.h"
# without spelling out the vendor path at every call site.
target_include_directories(${PROJECT_NAME} PRIVATE src extern/imgui-vr-helper)

# The mod version, as the VR helper surfaces it in its "Registered Clients" table — so a
# tester's screenshot of that table says which build they are on. Taken from the project
# version above rather than written out again.
target_compile_definitions(${PROJECT_NAME} PRIVATE ISEKAI_VERSION="${PROJECT_VERSION}")

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
