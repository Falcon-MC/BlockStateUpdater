cmake_minimum_required(VERSION 3.16)
project(Falcon.BlockStateUpdater LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(FALCON_SOURCE_DIR "" CACHE PATH "Local Falcon checkout used instead of fetching it from GitHub")

# nbt and binary streams come from Falcon.Shared in the Falcon repository
if(NOT TARGET FalconShared)
    if(FALCON_SOURCE_DIR)
        add_subdirectory(${FALCON_SOURCE_DIR}/Falcon.Shared ${CMAKE_CURRENT_BINARY_DIR}/_deps/falcon_shared-build)
    else()
        FetchContent_Declare(
            falcon
            GIT_REPOSITORY https://github.com/Falcon-MC/Falcon.git
            GIT_TAG dev
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
            SOURCE_SUBDIR Falcon.Shared
        )
        FetchContent_MakeAvailable(falcon)
    endif()
endif()

set(FALCON_BLOCK_UPGRADE_SCHEMA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Schemas
    CACHE INTERNAL "Directory containing the block state upgrade schema files")

set(FALCON_BLOCK_UPGRADE_LEGACY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Legacy
    CACHE INTERNAL "Directory containing the legacy numeric id and meta tables")

file(GLOB_RECURSE FALCON_BLOCK_STATE_UPDATER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(FalconBlockStateUpdater STATIC ${FALCON_BLOCK_STATE_UPDATER_SOURCES})

target_include_directories(FalconBlockStateUpdater PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(FalconBlockStateUpdater PUBLIC FalconShared)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(FALCON_BLOCK_STATE_UPDATER_BUILD_TESTS "Build the Falcon block state updater tests" ON)
else()
    option(FALCON_BLOCK_STATE_UPDATER_BUILD_TESTS "Build the Falcon block state updater tests" OFF)
endif()

if(FALCON_BLOCK_STATE_UPDATER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
