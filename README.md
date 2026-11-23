# virtual_filesystem

[![Linux CI](https://github.com/goloveshin/virtual_filesystem/actions/workflows/linux-ci.yml/badge.svg)](https://github.com/goloveshin/virtual_filesystem/actions/workflows/linux-ci.yml) [![Windows CI](https://github.com/goloveshin/virtual_filesystem/actions/workflows/windows-ci.yml/badge.svg)](https://github.com/goloveshin/virtual_filesystem/actions/workflows/windows-ci.yml)

A `std::function` stand-in for [`std::filesystem`](https://en.cppreference.com/w/cpp/filesystem).

`virtual_filesystem::filesystem_t` is the operations a backend must answer. `make_filesystem` fills them from `std::filesystem`. A test replaces any member with a fake. `std::filesystem::filesystem_error` does not leave a call: a logger runs first, then `on_error` receives the error. If `on_error` returns, the call returns a value-initialized result. If `on_error` throws, that exception leaves.

`exists`, `is_*`, `is_empty`, `relative`, and `proximate` are free functions. They are rules over the members, so a fake cannot disagree with them. `try_read`, `read`, and `write` carry the file bytes. `content_filter` runs on those bytes. An empty filter leaves them unchanged.

Documentation: <https://goloveshin.github.io/virtual_filesystem_doc/>

## Requirements

- C++20

## Use with CMake

```cmake
include(FetchContent)

FetchContent_Declare(
    virtual_filesystem
    GIT_REPOSITORY https://github.com/goloveshin/virtual_filesystem.git
    GIT_TAG main
)
FetchContent_MakeAvailable(virtual_filesystem)

target_link_libraries(my_app PRIVATE virtual_filesystem::virtual_filesystem)
```

```cpp
#include <virtual_filesystem/filesystem.hpp>
```

The unit tests and the example are built only when `virtual_filesystem` is the top-level project. To control it from a parent project, set the option `VIRTUAL_FILESYSTEM_BUILD_TESTS`.

## Build the tests

```text
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

CI builds and tests it on Linux (GCC, Clang) and Windows (MSVC), in Debug and Release.

## Layout

| path | what it is |
| --- | --- |
| `include/virtual_filesystem/` | the headers. include `filesystem.hpp` |
| `src/std_filesystem.cpp` | the `std::filesystem` backend |
| `example/` | code under test, a memory fake, and checks that use them. built and run with the tests |
| `tests/` | unit tests (doctest) |

## License

[Boost Software License 1.0](LICENSE)
