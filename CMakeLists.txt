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

# Task 5's FactoryPresets test target is appended here once it exists --
# deliberately not included yet, so this task's own build-verification step
# only references files that exist at this point in the plan.

# ── Install rules ─────────────────────────────────────────────────────────
# DPF writes all format outputs under a flat bin/ dir -- no Standalone
# target: Bastos is an effect, and this workspace only requires Standalone
# for instruments.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(_BIN "${CMAKE_BINARY_DIR}/bin")

install(DIRECTORY  "${_BIN}/Bastos.vst3"
        DESTINATION VST3
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

if(APPLE)
    install(DIRECTORY  "${_BIN}/Bastos.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            USE_SOURCE_PERMISSIONS)
else()
    install(FILES      "${_BIN}/Bastos.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

install(DIRECTORY  "${_BIN}/Bastos.lv2"
        DESTINATION LV2
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

install(PROGRAMS   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install.sh"
                   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/uninstall.sh"
        DESTINATION .
        COMPONENT   Runtime)

# ── CPack (TGZ) ─────────────────────────────────────────────────────────────
set(CPACK_GENERATOR                 TGZ)
set(CPACK_PACKAGE_NAME              Bastos)
set(CPACK_PACKAGE_VENDOR            Spellbound)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Bass enhancer and transient shaper")
set(CPACK_PACKAGE_VERSION           ${PROJECT_VERSION})
set(CPACK_SYSTEM_NAME               linux-x86_64)
set(CPACK_PACKAGE_FILE_NAME         "Bastos-${PROJECT_VERSION}-linux-x86_64")
set(CPACK_PACKAGING_INSTALL_PREFIX  "")
set(CPACK_STRIP_FILES               TRUE)
set(CPACK_PACKAGE_CHECKSUM          SHA256)
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_BINARY_DIR};${PROJECT_NAME};Runtime;/")
include(CPack)
