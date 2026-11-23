cmake_minimum_required(VERSION 4.0)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(virtual_filesystem LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED YES)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(VIRTUAL_FILESYSTEM_IS_TOP_LEVEL ON)
else()
    set(VIRTUAL_FILESYSTEM_IS_TOP_LEVEL OFF)
endif()

# the tests are built by default only when virtual_filesystem is the top-level project
option(VIRTUAL_FILESYSTEM_BUILD_TESTS "Build virtual_filesystem tests" ${VIRTUAL_FILESYSTEM_IS_TOP_LEVEL})

add_library(virtual_filesystem STATIC)
add_library(virtual_filesystem::virtual_filesystem ALIAS virtual_filesystem)

target_sources(
    virtual_filesystem
    PRIVATE
        src/std_filesystem.cpp
)

target_include_directories(
    virtual_filesystem
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(
    virtual_filesystem
    PUBLIC
        cxx_std_20
)

if(VIRTUAL_FILESYSTEM_BUILD_TESTS)
    if(NOT TARGET doctest::doctest)
        include(FetchContent)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
        FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.5.0
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(doctest)
    endif()

    add_executable(
        virtual_filesystem_tests
        tests/test_virtual_filesystem.cpp
    )
    target_link_libraries(
        virtual_filesystem_tests
        PRIVATE
            virtual_filesystem::virtual_filesystem
            doctest::doctest
    )
    target_compile_definitions(
        virtual_filesystem_tests
        PRIVATE
            DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
    )

    enable_testing()
    add_test(
        NAME virtual_filesystem_tests
        COMMAND virtual_filesystem_tests
    )

    add_subdirectory(example)
endif()
