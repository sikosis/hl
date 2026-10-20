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

add_library(hl_io STATIC src/io/input.cpp)
target_include_directories(hl_io PUBLIC src)
target_compile_features(hl_io PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(hl_io PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(hl src/main.cpp)
target_link_libraries(hl PRIVATE hl_io hl_platform)
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

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(platform_test PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(input_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME hl_version COMMAND hl --version)
    set_tests_properties(hl_version PROPERTIES PASS_REGULAR_EXPRESSION "hl ${PROJECT_VERSION}")

    add_test(NAME hl_help COMMAND hl --help)
    set_tests_properties(hl_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: hl")

    add_test(NAME platform COMMAND platform_test)
    add_test(NAME input COMMAND input_test)
endif()
