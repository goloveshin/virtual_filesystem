#pragma once

// include only from virtual_filesystem/filesystem.hpp

namespace virtual_filesystem
{

// true when status(path) exists
// throws: std::filesystem::filesystem_error
//
inline bool exists(const filesystem_t& filesystem, const path_t& path)
{
    const auto type = filesystem.status(path).type();
    return type != fs::file_type::none && type != fs::file_type::not_found;
}

// true when status(path) is a regular file. a symlink is followed
// throws: std::filesystem::filesystem_error
//
inline bool is_regular_file(const filesystem_t& filesystem, const path_t& path)
{
    return filesystem.status(path).type() == fs::file_type::regular;
}

// true when status(path) is a directory. a symlink is followed
// throws: std::filesystem::filesystem_error
//
inline bool is_directory(const filesystem_t& filesystem, const path_t& path)
{
    return filesystem.status(path).type() == fs::file_type::directory;
}

// true when status(path) is a block file. a symlink is followed
// throws: std::filesystem::filesystem_error
//
inline bool is_block_file(const filesystem_t& filesystem, const path_t& path)
{
    return filesystem.status(path).type() == fs::file_type::block;
}

// true when status(path) is a character file. a symlink is followed
// throws: std::filesystem::filesystem_error
//
inline bool is_character_file(const filesystem_t& filesystem, const path_t& path)
{
    return filesystem.status(path).type() == fs::file_type::character;
}

// true when status(path) is a fifo. a symlink is followed
// throws: std::filesystem::filesystem_error
//
inline bool is_fifo(const filesystem_t& filesystem, const path_t& path)
{
    return filesystem.status(path).type() == fs::file_type::fifo;
}

// true when status(path) is a socket. a symlink is followed
// throws: std::filesystem::filesystem_error
//
inline bool is_socket(const filesystem_t& filesystem, const path_t& path)
{
    return filesystem.status(path).type() == fs::file_type::socket;
}

// true when status(path) is some other kind of file. a symlink is followed
// throws: std::filesystem::filesystem_error
//
inline bool is_other(const filesystem_t& filesystem, const path_t& path)
{
    switch (filesystem.status(path).type())
    {
    case fs::file_type::none:
    case fs::file_type::not_found:
    case fs::file_type::regular:
    case fs::file_type::directory:
    case fs::file_type::symlink:
        return false;
    default:
        return true;
    }
}

// true when symlink_status(path) is a symlink
// throws: std::filesystem::filesystem_error
//
inline bool is_symlink(const filesystem_t& filesystem, const path_t& path)
{
    return filesystem.symlink_status(path).type() == fs::file_type::symlink;
}

// true when a file has size 0, or a directory has no entries
// throws: std::filesystem::filesystem_error
//
inline bool is_empty(const filesystem_t& filesystem, const path_t& path)
{
    if (filesystem.status(path).type() == fs::file_type::directory)
    {
        return filesystem.list({
            .root = path
        }).empty();
    }
    return filesystem.file_size(path) == 0;
}

// path relative to base. both are weakly canonical first
// throws: std::filesystem::filesystem_error
//
inline path_t relative(
    const filesystem_t& filesystem,
    const path_t& path,
    const path_t& base
)
{
    const auto canonical_path = filesystem.weakly_canonical(path);
    const auto canonical_base = filesystem.weakly_canonical(base);
    return canonical_path.lexically_relative(canonical_base);
}

// path proximate to base. both are weakly canonical first
// throws: std::filesystem::filesystem_error
//
inline path_t proximate(
    const filesystem_t& filesystem,
    const path_t& path,
    const path_t& base
)
{
    const auto canonical_path = filesystem.weakly_canonical(path);
    const auto canonical_base = filesystem.weakly_canonical(base);
    return canonical_path.lexically_proximate(canonical_base);
}

}// namespace virtual_filesystem
