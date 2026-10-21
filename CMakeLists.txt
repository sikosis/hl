cmake_minimum_required(VERSION 3.16)

project(hl VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(hl_platform STATIC)
target_include_directories(hl_platform PUBLIC src)
target_compile_features(hl_platform PUBLIC cxx_std_17)

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    target_sources(hl_platform PRIVATE src/platform/haiku/platform_haiku.cpp)
    target_link_libraries(hl_platform PUBLIC be)
else()
    target_sources(hl_platform PRIVATE src/platform/posix/platform_posix.cpp)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(hl_platform PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(hl_io STATIC
    src/io/entry_splitter.cpp
    src/io/input.cpp
)
target_include_directories(hl_io PUBLIC src)
target_compile_features(hl_io PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(hl_io PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(hl_model STATIC
    src/model/level.cpp
    src/model/record.cpp
)
target_include_directories(hl_model PUBLIC src)
target_compile_features(hl_model PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(hl_model PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(hl_parse STATIC
    src/parse/format.cpp
    src/parse/json_scanner.cpp
    src/parse/logfmt_scanner.cpp
    src/parse/scanner.cpp
    src/parse/timestamp.cpp
)
target_include_directories(hl_parse PUBLIC src)
target_compile_features(hl_parse PUBLIC cxx_std_17)
target_link_libraries(hl_parse PUBLIC hl_model)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(hl_parse PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(hl_format STATIC src/format/formatter.cpp)
target_include_directories(hl_format PUBLIC src)
target_compile_features(hl_format PUBLIC cxx_std_17)
target_link_libraries(hl_format PUBLIC hl_model)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(hl_format PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(hl_pipeline STATIC src/pipeline/processor.cpp)
target_include_directories(hl_pipeline PUBLIC src)
target_compile_features(hl_pipeline PUBLIC cxx_std_17)
target_link_libraries(hl_pipeline PUBLIC hl_format hl_io hl_parse)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(hl_pipeline PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(hl src/main.cpp)
target_link_libraries(hl PRIVATE hl_io hl_pipeline hl_platform)
target_compile_definitions(hl PRIVATE HL_VERSION="${PROJECT_VERSION}")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(hl PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS hl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(BUILD_TESTING)
    add_executable(platform_test tests/platform_test.cpp)
    target_link_libraries(platform_test PRIVATE hl_platform)

    add_executable(input_test tests/input_test.cpp)
    target_link_libraries(input_test PRIVATE hl_io)

    add_executable(format_test tests/format_test.cpp)
    target_link_libraries(format_test PRIVATE hl_parse)

    add_executable(scanner_test tests/scanner_test.cpp)
    target_link_libraries(scanner_test PRIVATE hl_parse)

    add_executable(timestamp_test tests/timestamp_test.cpp)
    target_link_libraries(timestamp_test PRIVATE hl_parse)

    add_executable(formatter_test tests/formatter_test.cpp)
    target_link_libraries(formatter_test PRIVATE hl_format hl_parse)

    add_executable(pipeline_test tests/pipeline_test.cpp)
    target_link_libraries(pipeline_test PRIVATE hl_pipeline)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(platform_test PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(input_test PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(format_test PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(scanner_test PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(timestamp_test PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(formatter_test PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(pipeline_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME hl_version COMMAND hl --version)
    set_tests_properties(hl_version PROPERTIES PASS_REGULAR_EXPRESSION "hl ${PROJECT_VERSION}")

    add_test(NAME hl_help COMMAND hl --help)
    set_tests_properties(hl_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: hl")

    add_test(NAME platform COMMAND platform_test)
    add_test(NAME input COMMAND input_test)
    add_test(NAME format COMMAND format_test)
    add_test(NAME scanner COMMAND scanner_test)
    add_test(NAME timestamp COMMAND timestamp_test)
    add_test(NAME formatter COMMAND formatter_test)
    add_test(NAME pipeline COMMAND pipeline_test)
endif()
