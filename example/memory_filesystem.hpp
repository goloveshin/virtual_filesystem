#pragma once

// virtual_filesystem
#include <virtual_filesystem/filesystem.hpp>

// std
#include <cstdint>
#include <iterator>
#include <map>
#include <system_error>


namespace example
{

// a filesystem of files in memory, for a test.
// it fills only the members that the code under test calls:
// status, file_size, create_directories, list, try_read, read, write.
// directories are not stored: every path above a file is a directory only by name,
// and status answers regular or not_found
//
struct memory_t
{
    std::map<virtual_filesystem::path_t, virtual_filesystem::bytes_t> files;

    // the filesystem reads and writes this object, so the object must outlive it
    //
    virtual_filesystem::filesystem_t filesystem()
    {
        namespace vfs = virtual_filesystem;
        namespace fs = vfs::fs;

        vfs::filesystem_t result;

        result.status = [this](const vfs::path_t& path)
        {
            return files.contains(path)
                ? fs::file_status{fs::file_type::regular}
                : fs::file_status{fs::file_type::not_found};
        };

        result.file_size = [this](const vfs::path_t& path)
        {
            const auto found = files.find(path);
            if (found == files.end())
            {
                throw fs::filesystem_error{
                    "file_size",
                    path,
                    std::make_error_code(std::errc::no_such_file_or_directory)
                };
            }
            return static_cast<std::uintmax_t>(found->second.size());
        };

        result.create_directories = [](const vfs::path_t&)
        {
            return true;
        };

        result.list = [this](const vfs::list_t& args)
        {
            vfs::entries_t entries;
            for (const auto& [path, bytes] : files)
            {
                const auto relative = path.lexically_relative(args.root);
                if (relative.empty() || *relative.begin() == "..")
                {
                    continue;
                }
                const auto depth = std::distance(relative.begin(), relative.end());
                if (!args.options.recursive && depth != 1)
                {
                    continue;
                }
                const auto regular = fs::file_status{fs::file_type::regular};
                entries.push_back({
                    .path = path,
                    .status = regular,
                    .symlink_status = regular
                });
            }
            return entries;
        };

        result.try_read = [this](const vfs::path_t& path) -> vfs::bytes_ot
        {
            const auto found = files.find(path);
            if (found == files.end())
            {
                return {};
            }
            return found->second;
        };

        result.read = [this](const vfs::path_t& path)
        {
            const auto found = files.find(path);
            if (found == files.end())
            {
                throw fs::filesystem_error{
                    "read",
                    path,
                    std::make_error_code(std::errc::no_such_file_or_directory)
                };
            }
            return found->second;
        };

        result.write = [this](const vfs::write_t& args)
        {
            files[args.path] = args.bytes;
        };

        return result;
    }
};

}// namespace example
