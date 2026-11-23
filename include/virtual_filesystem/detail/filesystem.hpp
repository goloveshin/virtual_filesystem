#pragma once

// include only from virtual_filesystem/filesystem.hpp

// std
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>


namespace virtual_filesystem
{

namespace fs = std::filesystem;

using path_t = fs::path;

// the bytes of a file
//
using bytes_t = std::string;
using bytes_ot = std::optional<bytes_t>;

// one directory entry, captured when it was listed.
// status follows a symlink. symlink_status does not
//
struct entry_t
{
    path_t path;
    fs::file_status status;
    fs::file_status symlink_status;
};
using entries_t = std::vector<entry_t>;

// how list walks from root
//
struct list_options_t
{
    bool recursive = false;
    bool follow_directory_symlink = false;
    bool skip_permission_denied = false;
};

struct list_t
{
    path_t root;
    list_options_t options = {};
};
using list_ft = std::function<entries_t (const list_t&)>;

using status_ft = std::function<fs::file_status (const path_t&)>;
using symlink_status_ft = std::function<fs::file_status (const path_t&)>;
using file_size_ft = std::function<std::uintmax_t (const path_t&)>;
using hard_link_count_ft = std::function<std::uintmax_t (const path_t&)>;
using last_write_time_ft = std::function<fs::file_time_type (const path_t&)>;
using space_ft = std::function<fs::space_info (const path_t&)>;

// true when path_1 and path_2 name the same file
//
struct equivalent_t
{
    path_t path_1;
    path_t path_2;
};
using equivalent_ft = std::function<bool (const equivalent_t&)>;

using read_symlink_ft = std::function<path_t (const path_t&)>;

using current_path_ft = std::function<path_t ()>;
using set_current_path_ft = std::function<void (const path_t&)>;
using absolute_ft = std::function<path_t (const path_t&)>;
using canonical_ft = std::function<path_t (const path_t&)>;
using weakly_canonical_ft = std::function<path_t (const path_t&)>;
using temp_directory_ft = std::function<path_t ()>;

// true when the directory was created
using create_directory_ft = std::function<bool (const path_t&)>;
// true when at least one directory was created
using create_directories_ft = std::function<bool (const path_t&)>;
// true when the file or the empty directory was removed
using remove_ft = std::function<bool (const path_t&)>;
// how many files and directories were removed
using remove_all_ft = std::function<std::uintmax_t (const path_t&)>;

struct rename_t
{
    path_t from;
    path_t to;
};
using rename_ft = std::function<void (const rename_t&)>;

struct resize_file_t
{
    path_t path;
    std::uintmax_t new_size = 0;
};
using resize_file_ft = std::function<void (const resize_file_t&)>;

// copy_options selects recursion, overwrite, and how symlinks are copied
//
struct copy_t
{
    path_t from;
    path_t to;
    fs::copy_options options = fs::copy_options::none;
};
using copy_ft = std::function<void (const copy_t&)>;

struct create_symlink_t
{
    path_t target;
    path_t link;
};
using create_symlink_ft = std::function<void (const create_symlink_t&)>;
using create_directory_symlink_ft = std::function<void (const create_symlink_t&)>;

struct create_hard_link_t
{
    path_t target;
    path_t link;
};
using create_hard_link_ft = std::function<void (const create_hard_link_t&)>;

struct permissions_t
{
    path_t path;
    fs::perms permissions = fs::perms::none;
    fs::perm_options options = fs::perm_options::replace;
};
using permissions_ft = std::function<void (const permissions_t&)>;

struct set_last_write_time_t
{
    path_t path;
    fs::file_time_type time;
};
using set_last_write_time_ft = std::function<void (const set_last_write_time_t&)>;

// the file bytes, or empty when the file cannot be opened. that is not an error
//
using try_read_ft = std::function<bytes_ot (const path_t&)>;
// the file bytes
//
using read_ft = std::function<bytes_t (const path_t&)>;

// replaces the bytes of the file
//
struct write_t
{
    path_t path;
    bytes_t bytes;
};
using write_ft = std::function<void (const write_t&)>;

// which side is handing the bytes to the filter
//
enum class content_filter_direction_et
{
    READ,
    WRITE
};

// path, bytes, and direction in. bytes out. empty: read and write keep the bytes
//
using content_filter_ft = std::function<bytes_t (
    const path_t&,
    const bytes_t&,
    content_filter_direction_et
)>;

// the calls a filesystem must answer. a fake replaces any of them
// throws: std::filesystem::filesystem_error
//
struct filesystem_t
{
    status_ft status;
    symlink_status_ft symlink_status;
    file_size_ft file_size;
    hard_link_count_ft hard_link_count;
    last_write_time_ft last_write_time;
    space_ft space;
    equivalent_ft equivalent;
    read_symlink_ft read_symlink;

    current_path_ft current_path;
    set_current_path_ft set_current_path;
    absolute_ft absolute;
    canonical_ft canonical;
    weakly_canonical_ft weakly_canonical;
    temp_directory_ft temp_directory;

    create_directory_ft create_directory;
    create_directories_ft create_directories;
    remove_ft remove;
    remove_all_ft remove_all;
    rename_ft rename;
    resize_file_ft resize_file;
    copy_ft copy;
    create_symlink_ft create_symlink;
    create_directory_symlink_ft create_directory_symlink;
    create_hard_link_ft create_hard_link;
    permissions_ft permissions;
    set_last_write_time_ft set_last_write_time;

    list_ft list;

    try_read_ft try_read;
    read_ft read;
    write_ft write;
    // path, bytes, and direction in. bytes out. empty: read and write keep the bytes
    content_filter_ft content_filter;
};

// wraps try_read, read, and write with content_filter.
// an empty filter leaves them as they are.
// try_read calls the filter only when it has bytes.
// make_filesystem does this before it returns
//
inline void apply_content_filter(filesystem_t& filesystem)
{
    const auto filter = filesystem.content_filter;
    if (!filter)
    {
        return;
    }

    const auto try_read = filesystem.try_read;
    const auto read = filesystem.read;
    const auto write = filesystem.write;

    filesystem.try_read = [filter, try_read](const path_t& path) -> bytes_ot
    {
        auto bytes = try_read(path);
        if (bytes)
        {
            *bytes = filter(path, *bytes, content_filter_direction_et::READ);
        }
        return bytes;
    };
    filesystem.read = [filter, read](const path_t& path)
    {
        return filter(path, read(path), content_filter_direction_et::READ);
    };
    filesystem.write = [filter, write](const write_t& args)
    {
        write({
            .path = args.path,
            .bytes = filter(args.path, args.bytes, content_filter_direction_et::WRITE)
        });
    };
}

}// namespace virtual_filesystem
