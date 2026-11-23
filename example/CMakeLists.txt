add_executable(
    virtual_filesystem_example
    main.cpp
)
target_link_libraries(
    virtual_filesystem_example
    PRIVATE
        virtual_filesystem::virtual_filesystem
)

add_test(
    NAME virtual_filesystem_example
    COMMAND virtual_filesystem_example
)
