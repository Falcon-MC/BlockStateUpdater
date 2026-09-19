cmake_minimum_required(VERSION 3.16)
project(Falcon.BlockStateUpdater LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(FALCON_NBT_SOURCE_DIR "" CACHE PATH "Local Falcon NBT checkout used instead of fetching it from GitHub")

if(NOT TARGET FalconNBT)
    if(FALCON_NBT_SOURCE_DIR)
        add_subdirectory(${FALCON_NBT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/_deps/falcon_nbt-build)
    else()
        FetchContent_Declare(
            falcon_nbt
            GIT_REPOSITORY https://github.com/Falcon-MC/NBT.git
            GIT_TAG main
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        FetchContent_MakeAvailable(falcon_nbt)
    endif()
endif()

set(FALCON_BLOCK_UPGRADE_SCHEMA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Schemas
    CACHE INTERNAL "Directory containing the block state upgrade schema files")

file(GLOB_RECURSE FALCON_BLOCK_STATE_UPDATER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(FalconBlockStateUpdater STATIC ${FALCON_BLOCK_STATE_UPDATER_SOURCES})

target_include_directories(FalconBlockStateUpdater PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(FalconBlockStateUpdater PUBLIC FalconNBT)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(FALCON_BLOCK_STATE_UPDATER_BUILD_TESTS "Build the Falcon block state updater tests" ON)
else()
    option(FALCON_BLOCK_STATE_UPDATER_BUILD_TESTS "Build the Falcon block state updater tests" OFF)
endif()

if(FALCON_BLOCK_STATE_UPDATER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
