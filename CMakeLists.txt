cmake_minimum_required(VERSION 3.14)
project(localdb VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Options ──────────────────────────────────────────────────
option(LOCALDB_BUILD_TESTS    "Build unit tests"   ON)
option(LOCALDB_BUILD_EXAMPLES "Build examples"      ON)
option(LOCALDB_BUILD_SHARED   "Build shared (.dll/.so)" OFF)

# ── Compiler flags for performance ───────────────────────────
if(MSVC)
    add_compile_options(/O2 /W3 /D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-O3 -Wall -Wextra -march=native)
endif()

# ── Core library ─────────────────────────────────────────────
set(LOCALDB_SOURCES
    src/core/database.c
    src/storage/btree.c
    src/storage/mmap_pager.c
    src/storage/hp_wal.c
    src/memory/arena.c
    src/util/json.c
    src/util/string.c
    src/vfs/vfs.c
)

if(LOCALDB_BUILD_SHARED)
    add_library(localdb SHARED ${LOCALDB_SOURCES})
    if(WIN32)
        set_target_properties(localdb PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
else()
    add_library(localdb STATIC ${LOCALDB_SOURCES})
endif()

target_include_directories(localdb PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(WIN32)
    target_compile_definitions(localdb PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(localdb PRIVATE pthread)
endif()

# ── Tests ────────────────────────────────────────────────────
if(LOCALDB_BUILD_TESTS)
    enable_testing()
    add_executable(localdb_tests
        tests/test_main.c
    )
    target_link_libraries(localdb_tests PRIVATE localdb)
    target_include_directories(localdb_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    add_test(NAME localdb_tests COMMAND localdb_tests)
endif()

# ── Examples ─────────────────────────────────────────────────
if(LOCALDB_BUILD_EXAMPLES)
    add_executable(example_basic examples/basic.c)
    target_link_libraries(example_basic PRIVATE localdb)

    add_executable(example_agent_memory examples/agent_memory.c)
    target_link_libraries(example_agent_memory PRIVATE localdb)
endif()

# ── Install ──────────────────────────────────────────────────
install(TARGETS localdb
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES include/localdb.h DESTINATION include)
