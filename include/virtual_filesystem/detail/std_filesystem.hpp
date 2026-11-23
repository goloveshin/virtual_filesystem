#pragma once

// include only from virtual_filesystem/filesystem.hpp

// std
#include <functional>


namespace virtual_filesystem
{

// which filesystem_t call is about to run.
// ABSOLUTE_PATH is not named ABSOLUTE: wingdi.h defines that macro
//
enum class operation_et
{
    STATUS,
    SYMLINK_STATUS,
    FILE_SIZE,
    HARD_LINK_COUNT,
    LAST_WRITE_TIME,
    SPACE,
    EQUIVALENT,
    READ_SYMLINK,
    CURRENT_PATH,
    SET_CURRENT_PATH,
    ABSOLUTE_PATH,
    CANONICAL,
    WEAKLY_CANONICAL,
    TEMP_DIRECTORY,
    CREATE_DIRECTORY,
    CREATE_DIRECTORIES,
    REMOVE,
    REMOVE_ALL,
    RENAME,
    RESIZE_FILE,
    COPY,
    CREATE_SYMLINK,
    CREATE_DIRECTORY_SYMLINK,
    CREATE_HARD_LINK,
    PERMISSIONS,
    SET_LAST_WRITE_TIME,
    LIST,
    TRY_READ,
    READ,
    WRITE
};

// path is the file the call names. other is set when the call names two paths
//
struct log_t
{
    operation_et operation = operation_et::STATUS;
    path_t path;
    path_t other;
};
using logger_ft = std::function<void (const log_t&)>;

// a filesystem_error caught from std::filesystem.
// the callback may throw its own exception. that exception is not caught
//
using error_ft = std::function<void (const fs::filesystem_error&)>;

// fills filesystem_t from std::filesystem.
// filesystem_error is caught and passed to on_error.
// when on_error returns, the call returns a value-initialized result.
// an empty logger or an empty on_error is skipped.
// content_filter is applied to try_read, read, and write.
// an empty filter leaves the bytes as they are
//
struct make_filesystem_t
{
    logger_ft logger;
    error_ft on_error;
    content_filter_ft content_filter;
};
filesystem_t make_filesystem(const make_filesystem_t&);

}// namespace virtual_filesystem
