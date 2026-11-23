#pragma once

// virtual_filesystem
#include <virtual_filesystem/filesystem.hpp>

// std
#include <algorithm>
#include <string>
#include <vector>


namespace example
{

// the code under test.
// it takes a filesystem_t and does not know what is behind it:
// the disk, a memory fake, or a log
//

// true when dir has a regular file config.txt
//
inline bool has_config(
    const virtual_filesystem::filesystem_t& filesystem,
    const virtual_filesystem::path_t& dir
)
{
    return virtual_filesystem::is_regular_file(filesystem, dir / "config.txt");
}

// the text of dir/config.txt, or an empty string when there is none
//
inline std::string load_config(
    const virtual_filesystem::filesystem_t& filesystem,
    const virtual_filesystem::path_t& dir
)
{
    return filesystem.try_read(dir / "config.txt").value_or("");
}

// writes dir/config.txt. the directory is created when it is missing
//
inline void save_config(
    const virtual_filesystem::filesystem_t& filesystem,
    const virtual_filesystem::path_t& dir,
    const std::string& text
)
{
    filesystem.create_directories(dir);
    filesystem.write({
        .path = dir / "config.txt",
        .bytes = text
    });
}

// the .cpp files under root, nested ones too, sorted by path
//
inline std::vector<std::string> source_files(
    const virtual_filesystem::filesystem_t& filesystem,
    const virtual_filesystem::path_t& root
)
{
    namespace fs = virtual_filesystem::fs;

    const auto entries = filesystem.list({
        .root = root,
        .options = {.recursive = true}
    });

    std::vector<std::string> result;
    for (const auto& entry : entries)
    {
        if (entry.status.type() == fs::file_type::regular && entry.path.extension() == ".cpp")
        {
            result.push_back(entry.path.generic_string());
        }
    }
    std::ranges::sort(result);
    return result;
}

}// namespace example
