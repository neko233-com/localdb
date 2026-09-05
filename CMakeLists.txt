cmake_minimum_required(VERSION 3.14)
project(localdb VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Options ──────────────────────────────────────────────────
option(LOCALDB_BUILD_TESTS    "Build unit tests"   ON)
option(LOCALDB_BUILD_EXAMPLES "Build examples"      ON)
option(LOCALDB_BUILD_SHARED   "Build shared (.dll/.so)" OFF)

# ── Core library ─────────────────────────────────────────────
set(LOCALDB_SOURCES
    src/core/database.c
    src/core/error.c
    src/storage/pager.c
    src/storage/btree.c
    src/storage/wal.c
    src/storage/page_cache.c
    src/sql/lexer.c
    src/sql/parser.c
    src/sql/executor.c
    src/txn/transaction.c
    src/vfs/vfs.c
    src/util/json.c
    src/util/string.c
)

if(LOCALDB_BUILD_SHARED)
    add_library(localdb SHARED ${LOCALDB_SOURCES})
    set_target_properties(localdb PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
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
        tests/test_open_close.c
        tests/test_document.c
        tests/test_sql.c
        tests/test_batch.c
        tests/test_ttl.c
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
