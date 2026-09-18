cmake_minimum_required(VERSION 3.22)
project(Bastos VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# DPF has no tagged releases -- pin the exact commit SHA Hex/Pugilist/Tank/
# Outflank all use.
set(AUDIOPLUGINS_DPF_GIT_TAG "4238e1c7f0351bbe488d79f0899c540543ac7583" CACHE STRING "Pinned DPF commit")

# CAVEAT: the patch step only runs on a fresh population of build/_deps/dpf-src.
# Delete it (or all of build/) when you need to be sure the patches took effect;
# cmake/apply_patch.cmake is idempotent but that's not a substitute for a clean re-fetch.
FetchContent_Declare(dpf
    GIT_REPOSITORY https://github.com/DISTRHO/DPF.git
    GIT_TAG        ${AUDIOPLUGINS_DPF_GIT_TAG}
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-state-chunked-read.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
            COMMAND ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-activate-latency.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
)
FetchContent_MakeAvailable(dpf)

set(BASTOS_DPF_TARGETS vst3 clap lv2)
if(APPLE)
    list(APPEND BASTOS_DPF_TARGETS au)
endif()

dpf_add_plugin(Bastos
    TARGETS ${BASTOS_DPF_TARGETS}
    UI_TYPE opengl
    USE_FILE_BROWSER TRUE
    FILES_DSP
        Source/BastosPluginAdapter.cpp
        Source/DSP/TransientShaperCore.cpp
    FILES_UI
        Source/BastosUI.cpp
)

target_include_directories(Bastos PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# DPF's dpf_add_plugin() unconditionally compiles a WebViewImpl.cpp that uses
# the GNU `typeof` extension and fails under strict ISO C++ -- same workaround
# every prior migration in this workspace needs (see Hex's CMakeLists.txt).
if(TARGET dgl-opengl)
    set_target_properties(dgl-opengl PROPERTIES CXX_EXTENSIONS ON)
endif()

# ── Shared HUI library ────────────────────────────────────────────────────
# ORDERING (load-bearing): must come after dpf_add_plugin(Bastos ...), which
# is what lazily creates DPF's dgl-opengl target that AudioPluginsCommon::
# hui_dgl is guarded on.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(AudioPluginsCommon
    GIT_REPOSITORY https://github.com/TriYop/spellbound-common.git
    GIT_TAG        v0.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(AudioPluginsCommon)

# Linked into `Bastos-ui` (not `Bastos`): dpf_add_plugin() splits the plugin
# into a common `Bastos` static lib plus `Bastos-dsp`/`Bastos-ui`; BastosUI.cpp
# is the only consumer of the widgets/presets and lives in `Bastos-ui`.
target_link_libraries(Bastos-ui PRIVATE AudioPluginsCommon::hui_dgl AudioPluginsCommon::presets)
target_link_libraries(Bastos PUBLIC AudioPluginsCommon::io)

# ── Unit tests (no JUCE/DPF dependency) ──────────────────────────────────
enable_testing()

add_executable(test_transientshapercore
    Tests/test_transientshapercore.cpp
    Source/DSP/TransientShaperCore.cpp
)
target_include_directories(test_transientshapercore PRIVATE Source/ Tests/)
target_compile_features(test_transientshapercore PRIVATE cxx_std_20)
add_test(NAME TransientShaperCore COMMAND test_transientshapercore)

add_executable(test_factorypresets Tests/test_factorypresets.cpp)
target_include_directories(test_factorypresets PRIVATE Source/ Tests/)
target_link_libraries(test_factorypresets PRIVATE AudioPluginsCommon::presets)
target_compile_features(test_factorypresets PRIVATE cxx_std_20)
add_test(NAME FactoryPresets COMMAND test_factorypresets)

# ── Install rules ─────────────────────────────────────────────────────────
# DPF writes all format outputs under a flat bin/ dir -- no Standalone
# target: Bastos is an effect, and this workspace only requires Standalone
# for instruments.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(PACKAGE_DEB "Package as a Debian .deb (FHS /usr paths) instead of a portable relative-path tarball" OFF)

if(PACKAGE_DEB)
    set(_DEST_VST3 lib/vst3)
    set(_DEST_CLAP lib/clap)
    set(_DEST_LV2  lib/lv2)
else()
    set(_DEST_VST3 VST3)
    set(_DEST_CLAP CLAP)
    set(_DEST_LV2  LV2)
endif()

set(_BIN "${CMAKE_BINARY_DIR}/bin")

install(DIRECTORY  "${_BIN}/Bastos.vst3"
        DESTINATION ${_DEST_VST3}
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

if(APPLE)
    install(DIRECTORY  "${_BIN}/Bastos.clap"
            DESTINATION ${_DEST_CLAP}
            COMPONENT   Runtime
            USE_SOURCE_PERMISSIONS)
else()
    install(FILES      "${_BIN}/Bastos.clap"
            DESTINATION ${_DEST_CLAP}
            COMPONENT   Runtime
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

install(DIRECTORY  "${_BIN}/Bastos.lv2"
        DESTINATION ${_DEST_LV2}
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

if(NOT PACKAGE_DEB)
    install(PROGRAMS   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install.sh"
                       "${CMAKE_CURRENT_SOURCE_DIR}/scripts/uninstall.sh"
            DESTINATION .
            COMPONENT   Runtime)
endif()

if(PACKAGE_DEB)
    # ── CPack (Debian .deb) ─────────────────────────────────────────────────
    set(CPACK_GENERATOR                    DEB)
    set(CPACK_PACKAGE_NAME                 spellbound-bastos)
    set(CPACK_PACKAGE_VENDOR               spellbound)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY  "Spellbound Havoc — Transient Devastation")
    set(CPACK_PACKAGE_VERSION              ${PROJECT_VERSION})
    set(CPACK_PACKAGING_INSTALL_PREFIX     "/usr")
    set(CPACK_STRIP_FILES                  TRUE)
    set(CPACK_PACKAGE_CHECKSUM              SHA256)
    set(CPACK_PACKAGE_CONTACT              "TriYop <dev@yvanjanet.net>")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER    "TriYop <dev@yvanjanet.net>")
    set(CPACK_DEBIAN_PACKAGE_SECTION       sound)
    set(CPACK_DEBIAN_PACKAGE_PRIORITY      optional)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS     TRUE)
    set(CPACK_DEBIAN_FILE_NAME             DEB-DEFAULT)
else()
    # ── CPack (TGZ) ───────────────────────────────────────────────────────────
    set(CPACK_GENERATOR                 TGZ)
    set(CPACK_PACKAGE_NAME              Bastos)
    set(CPACK_PACKAGE_VENDOR            Spellbound)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Spellbound Havoc — Transient Devastation")
    set(CPACK_PACKAGE_VERSION           ${PROJECT_VERSION})
    set(CPACK_SYSTEM_NAME               linux-x86_64)
    set(CPACK_PACKAGE_FILE_NAME         "Bastos-${PROJECT_VERSION}-linux-x86_64")
    set(CPACK_PACKAGING_INSTALL_PREFIX  "")
    set(CPACK_STRIP_FILES               TRUE)
    set(CPACK_PACKAGE_CHECKSUM          SHA256)
endif()
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_BINARY_DIR};${PROJECT_NAME};Runtime;/")
include(CPack)
