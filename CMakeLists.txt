cmake_minimum_required(VERSION 3.22)

# Work around dependencies that still declare old cmake_minimum_required
# values (nlohmann/json, lunasvg) when building with CMake >= 4.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(levi_create_android LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Mod identity
# ---------------------------------------------------------------------------
set(MOD_ID "levi-create" CACHE STRING "LeviLauncher mod package directory name (runtime mod id)")
set(MOD_NAME "LeviCreate - Voxel Studio" CACHE STRING "LeviLauncher mod display name")
set(MOD_AUTHOR "LeviCreate Team" CACHE STRING "LeviLauncher mod author")
set(MOD_VERSION "1.0.0" CACHE STRING "LeviLauncher mod version")
set(MOD_LIBRARY_NAME "levi_create" CACHE STRING "Native library name without lib prefix / .so suffix")
set(MOD_MINECRAFT_VERSIONS "[]" CACHE STRING "Raw JSON array for manifest minecraft_versions")
set(MOD_ICON "icon.png" CACHE STRING "Optional package-relative icon path")

# ---------------------------------------------------------------------------
# Dependencies (host + android)
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_Declare(
    boost_pfr
    GIT_REPOSITORY https://github.com/boostorg/pfr.git
    GIT_TAG boost-1.88.0
)
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.7
)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 11.2.0
)
set(FMT_DOC OFF CACHE BOOL "Disable fmt docs" FORCE)
set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Disable fmt install rules" FORCE)
FetchContent_MakeAvailable(nlohmann_json boost_pfr magic_enum fmt)

# Preloader SDK: either point PRELOADER_ANDROID_ROOT at a checkout of
# https://github.com/LiteLDev/preloader-android (tag 0.2.2) or let
# FetchContent download it.
set(PRELOADER_ANDROID_ROOT "" CACHE PATH "Path to preloader-android source (tag 0.2.2)")
if(NOT PRELOADER_ANDROID_ROOT)
    FetchContent_Declare(
        preloader_android
        GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
        GIT_TAG 0.2.2
    )
    FetchContent_GetProperties(preloader_android)
    if(NOT preloader_android_POPULATED)
        FetchContent_Populate(preloader_android)
    endif()
    set(PRELOADER_ANDROID_ROOT "${preloader_android_SOURCE_DIR}")
endif()
set(LEVI_PRELOADER_INCLUDE_DIR "${PRELOADER_ANDROID_ROOT}/include")

# ---------------------------------------------------------------------------
# Host branch: build the config generator that emits config.json + schema
# ---------------------------------------------------------------------------
if(NOT ANDROID)
    set(LEVI_GENERATED_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated-config")

    add_executable(levi_config_generator
        src/config_generator.cpp
        src/mod/Config.cpp
    )
    target_include_directories(levi_config_generator PRIVATE
        src
        "${LEVI_PRELOADER_INCLUDE_DIR}"
    )
    target_compile_definitions(levi_config_generator PRIVATE PL_CONFIG_NO_RUNTIME)
    target_link_libraries(levi_config_generator PRIVATE
        nlohmann_json::nlohmann_json
        Boost::pfr
        magic_enum::magic_enum
    )

    add_custom_target(levi_generate_config
        COMMAND levi_config_generator "${LEVI_GENERATED_CONFIG_DIR}"
        DEPENDS levi_config_generator
        COMMENT "Generating config.json and config.schema.json"
        VERBATIM
    )

    return()
endif()

# ---------------------------------------------------------------------------
# Android branch
# ---------------------------------------------------------------------------
if(NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a" AND
   NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
    message(FATAL_ERROR "Unsupported Android ABI: ${CMAKE_ANDROID_ARCH_ABI}")
endif()

set(LEVI_PACKAGE_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config" CACHE PATH
    "Directory containing config.json and config.schema.json to package")
get_filename_component(LEVI_PACKAGE_CONFIG_DIR "${LEVI_PACKAGE_CONFIG_DIR}" ABSOLUTE)
set(LEVI_PACKAGE_CONFIG_JSON "${LEVI_PACKAGE_CONFIG_DIR}/config.json")
set(LEVI_PACKAGE_CONFIG_SCHEMA "${LEVI_PACKAGE_CONFIG_DIR}/config.schema.json")

if(NOT EXISTS "${LEVI_PACKAGE_CONFIG_JSON}")
    message(FATAL_ERROR "LEVI_PACKAGE_CONFIG_DIR must contain config.json: ${LEVI_PACKAGE_CONFIG_JSON}")
endif()
if(NOT EXISTS "${LEVI_PACKAGE_CONFIG_SCHEMA}")
    message(FATAL_ERROR "LEVI_PACKAGE_CONFIG_DIR must contain config.schema.json: ${LEVI_PACKAGE_CONFIG_SCHEMA}")
endif()

option(LEVI_LINK_PRELOADER
       "Build and link the preloader target instead of leaving SDK symbols for runtime lookup"
       ON)

if(LEVI_LINK_PRELOADER AND NOT TARGET preloader)
    add_subdirectory("${PRELOADER_ANDROID_ROOT}" "${CMAKE_BINARY_DIR}/_preloader_android")
endif()

add_library(${MOD_LIBRARY_NAME} SHARED
    src/main.cpp
    src/mod/Config.cpp
    src/mod/LeviCreateMod.cpp
    src/mod/VoxelStudio.cpp
    src/mod/voxel/VoxelStructure.cpp
    src/mod/voxel/VoxelTransform.cpp
    src/mod/voxel/VoxelIO.cpp
    src/mod/ui/Projection.cpp
    src/mod/ui/HudPresenter.cpp
    src/mod/ui/CanvasController.cpp
    src/mod/ui/ButtonSet.cpp
    src/mod/game/GameBridge.cpp
)

set_target_properties(${MOD_LIBRARY_NAME} PROPERTIES
    OUTPUT_NAME "${MOD_LIBRARY_NAME}"
)

target_include_directories(${MOD_LIBRARY_NAME} PRIVATE
    src
    "${LEVI_PRELOADER_INCLUDE_DIR}"
)

target_compile_options(${MOD_LIBRARY_NAME} PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU>:-fvisibility=hidden -fvisibility-inlines-hidden>
    $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra>
)

target_link_options(${MOD_LIBRARY_NAME} PRIVATE
    "-Wl,-z,max-page-size=16384"
)

find_library(log-lib log REQUIRED)
target_link_libraries(${MOD_LIBRARY_NAME} PRIVATE
    ${log-lib}
    nlohmann_json::nlohmann_json
    Boost::pfr
    magic_enum::magic_enum
    fmt::fmt
)

if(TARGET preloader)
    target_link_libraries(${MOD_LIBRARY_NAME} PRIVATE preloader)
else()
    target_link_options(${MOD_LIBRARY_NAME} PRIVATE
        "-Wl,--unresolved-symbols=ignore-all")
endif()

if(NOT CMAKE_STRIP)
    message(FATAL_ERROR "CMAKE_STRIP is required to strip the generated library.")
endif()

add_custom_command(TARGET ${MOD_LIBRARY_NAME} POST_BUILD
    COMMAND "${CMAKE_STRIP}" --strip-unneeded "$<TARGET_FILE:${MOD_LIBRARY_NAME}>"
    COMMENT "Stripping symbols from $<TARGET_FILE_NAME:${MOD_LIBRARY_NAME}>"
    VERBATIM
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/manifest.json"
    @ONLY
)

# ---------------------------------------------------------------------------
# .levipack packaging (folder-wrapped zip, official template layout)
# ---------------------------------------------------------------------------
set(LEVI_PACKAGE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/package")
set(LEVI_PACKAGE_MOD_DIR "${LEVI_PACKAGE_ROOT}/${MOD_ID}")
set(LEVI_PACKAGE_FILE "${CMAKE_CURRENT_BINARY_DIR}/${MOD_ID}-${MOD_VERSION}-${CMAKE_ANDROID_ARCH_ABI}.levipack")
set(LEVI_PACKAGE_ICON_COMMAND)
set(LEVI_PACKAGE_CONFIG_COMMAND
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${LEVI_PACKAGE_CONFIG_DIR}" "${LEVI_PACKAGE_MOD_DIR}/config")

if(NOT MOD_ICON STREQUAL "")
    if(IS_ABSOLUTE "${MOD_ICON}")
        message(FATAL_ERROR "MOD_ICON must be a package-relative path")
    endif()
    if(MOD_ICON MATCHES "(^|/)\\.\\.(/|$)")
        message(FATAL_ERROR "MOD_ICON must not contain ..")
    endif()

    set(MOD_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${MOD_ICON}")
    if(EXISTS "${MOD_ICON_SOURCE}")
        get_filename_component(MOD_ICON_DIR "${MOD_ICON}" DIRECTORY)
        if(NOT MOD_ICON_DIR STREQUAL "")
            list(APPEND LEVI_PACKAGE_ICON_COMMAND
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                        "${LEVI_PACKAGE_MOD_DIR}/${MOD_ICON_DIR}")
        endif()
        list(APPEND LEVI_PACKAGE_ICON_COMMAND
            COMMAND "${CMAKE_COMMAND}" -E copy
                    "${MOD_ICON_SOURCE}" "${LEVI_PACKAGE_MOD_DIR}/${MOD_ICON}")
    else()
        message(STATUS "Icon not found, packaging without icon: ${MOD_ICON_SOURCE}")
    endif()
endif()

add_custom_target(levi_package
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${LEVI_PACKAGE_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${LEVI_PACKAGE_MOD_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${MOD_LIBRARY_NAME}>"
            "${LEVI_PACKAGE_MOD_DIR}/lib${MOD_LIBRARY_NAME}.so"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/manifest.json"
            "${LEVI_PACKAGE_MOD_DIR}/manifest.json"
    ${LEVI_PACKAGE_ICON_COMMAND}
    ${LEVI_PACKAGE_CONFIG_COMMAND}
    COMMAND "${CMAKE_COMMAND}" -E chdir "${LEVI_PACKAGE_ROOT}"
            "${CMAKE_COMMAND}" -E tar "cf" "${LEVI_PACKAGE_FILE}"
            --format=zip "${MOD_ID}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS ${MOD_LIBRARY_NAME}
    COMMENT "Packaging ${MOD_ID} for LeviLauncher (.levipack)"
    VERBATIM
)
