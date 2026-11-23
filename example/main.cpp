// example
#include "config_loader.hpp"
#include "memory_filesystem.hpp"

// virtual_filesystem
#include <virtual_filesystem/filesystem.hpp>

// std
#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>


namespace vfs = virtual_filesystem;
namespace fs = virtual_filesystem::fs;

namespace
{

int failures = 0;

void check(const bool is_ok, const std::string& what)
{
    std::cout << (is_ok ? "ok      " : "FAILED  ") << what << "\n";
    if (!is_ok)
    {
        ++failures;
    }
}

// a directory removed when the example ends
//
struct temp_dir_t
{
    vfs::path_t path;

    temp_dir_t()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("virtual_filesystem_example_" + std::to_string(stamp));
        fs::create_directories(path);
    }

    ~temp_dir_t()
    {
        std::error_code error;
        fs::remove_all(path, error);
    }

    temp_dir_t(const temp_dir_t&) = delete;
    temp_dir_t& operator=(const temp_dir_t&) = delete;
};

// a filesystem whose status answers regular for one path only
//
vfs::filesystem_t filesystem_with_file(const vfs::path_t& file)
{
    vfs::filesystem_t filesystem;
    filesystem.status = [file](const vfs::path_t& path)
    {
        return path == file
            ? fs::file_status{fs::file_type::regular}
            : fs::file_status{fs::file_type::not_found};
    };
    return filesystem;
}

// replacing members of filesystem_t
//
void fakes()
{
    // a partial fake: the code under test calls status, so only status is set
    const auto partial = filesystem_with_file("dir/config.txt");
    check(example::has_config(partial, "dir"), "a fake status makes has_config true");
    check(!example::has_config(partial, "other"), "a fake status makes has_config false");

    // a member that is not set is an empty std::function
    vfs::filesystem_t empty;
    bool is_thrown = false;
    try
    {
        empty.file_size("a");
    }
    catch (const std::bad_function_call&)
    {
        is_thrown = true;
    }
    check(is_thrown, "an empty member throws std::bad_function_call");

    // a fake can fail the way std::filesystem fails
    vfs::filesystem_t denied;
    denied.status = [](const vfs::path_t& path) -> fs::file_status
    {
        throw fs::filesystem_error{
            "status",
            path,
            std::make_error_code(std::errc::permission_denied)
        };
    };
    is_thrown = false;
    try
    {
        example::has_config(denied, "dir");
    }
    catch (const fs::filesystem_error& error)
    {
        is_thrown = error.path1() == "dir/config.txt";
    }
    check(is_thrown, "a fake that throws filesystem_error reaches the caller");

    // one member of a real filesystem is replaced
    auto real = vfs::make_filesystem({});
    real.current_path = []
    {
        return vfs::path_t{"/work"};
    };
    check(real.current_path() == "/work", "a member of a made filesystem can be replaced");

    // a filesystem in memory
    example::memory_t memory;
    const auto filesystem = memory.filesystem();
    check(example::load_config(filesystem, "dir").empty(), "memory: no config is an empty string");
    example::save_config(filesystem, "dir", "mode=fast");
    check(example::load_config(filesystem, "dir") == "mode=fast", "memory: save_config, then load_config");
    check(example::has_config(filesystem, "dir"), "memory: has_config sees the saved file");
    check(filesystem.file_size("dir/config.txt") == 9, "memory: file_size is the size of the bytes");

    memory.files["src/a.cpp"] = "";
    memory.files["src/a.hpp"] = "";
    memory.files["src/sub/b.cpp"] = "";
    const std::vector<std::string> expected{"src/a.cpp", "src/sub/b.cpp"};
    check(example::source_files(filesystem, "src") == expected, "memory: source_files lists nested .cpp files");
}

// the queries are rules over the members
//
void queries()
{
    vfs::filesystem_t symlink;
    symlink.status = [](const vfs::path_t&)
    {
        return fs::file_status{fs::file_type::regular};
    };
    symlink.symlink_status = [](const vfs::path_t&)
    {
        return fs::file_status{fs::file_type::symlink};
    };
    check(vfs::is_regular_file(symlink, "l"), "is_regular_file follows a symlink: it reads status");
    check(vfs::is_symlink(symlink, "l"), "is_symlink does not follow it: it reads symlink_status");

    vfs::filesystem_t directory;
    directory.status = [](const vfs::path_t&)
    {
        return fs::file_status{fs::file_type::directory};
    };
    directory.list = [](const vfs::list_t&)
    {
        return vfs::entries_t{};
    };
    check(vfs::is_empty(directory, "d"), "is_empty is true for a directory without entries");

    vfs::filesystem_t canonical;
    canonical.weakly_canonical = [](const vfs::path_t& path)
    {
        return path;
    };
    check(vfs::relative(canonical, "/a/b/c", "/a") == vfs::path_t{"b/c"}, "relative canonicalizes, then cuts the base");
    check(vfs::relative(canonical, "/x/y", "/a") == vfs::path_t{"../x/y"}, "relative may climb with ..");

    // "x/y" is relative and the base is absolute: there is no relative path between them
    check(vfs::relative(canonical, "x/y", "/a").empty(), "relative is empty when there is no relative path");
    check(vfs::proximate(canonical, "x/y", "/a") == vfs::path_t{"x/y"}, "proximate returns the path itself then");
}

// make_filesystem on a real directory: logger, on_error, and the result of a failed call
//
void errors()
{
    temp_dir_t dir;
    const auto missing = dir.path / "missing.txt";

    std::vector<std::string> events;
    const auto filesystem = vfs::make_filesystem({
        .logger = [&](const vfs::log_t& log)
        {
            events.push_back("log");
            if (log.operation == vfs::operation_et::RENAME)
            {
                events.push_back("rename to " + log.other.filename().string());
            }
        },
        .on_error = [&](const fs::filesystem_error& error)
        {
            events.push_back("error " + error.path1().filename().string());
        }
    });

    // a missing file is not an error for status
    check(!vfs::exists(filesystem, missing), "status of a missing file is not an error");
    check(events == std::vector<std::string>{"log"}, "the logger ran, on_error did not");

    // file_size of a missing file is an error: it is logged, then handled, and the result is 0
    events.clear();
    check(filesystem.file_size(missing) == 0, "a failed file_size returns 0");
    const std::vector<std::string> log_then_error{"log", "error missing.txt"};
    check(events == log_then_error, "the logger runs before on_error");

    // a call with two paths puts the second one in log_t::other
    events.clear();
    filesystem.rename({
        .from = missing,
        .to = dir.path / "renamed.txt"
    });
    const std::vector<std::string> rename_events{"log", "rename to renamed.txt", "error missing.txt"};
    check(events == rename_events, "rename logs both paths, then on_error runs");

    // try_read of a missing file is empty and is not an error
    events.clear();
    check(!filesystem.try_read(missing).has_value(), "try_read of a missing file is empty");
    check(events == std::vector<std::string>{"log"}, "try_read of a missing file does not call on_error");

    // read of a missing file is an error
    events.clear();
    check(filesystem.read(missing).empty(), "a failed read returns empty bytes");
    check(events == log_then_error, "a failed read calls on_error");

    // empty callbacks: the call neither logs nor throws
    const auto quiet = vfs::make_filesystem({});
    bool is_thrown = false;
    try
    {
        quiet.file_size(missing);
    }
    catch (...)
    {
        is_thrown = true;
    }
    check(!is_thrown && quiet.file_size(missing) == 0, "without on_error a failed call returns a value-initialized result");

    // on_error may throw its own exception
    const auto strict = vfs::make_filesystem({
        .on_error = [](const fs::filesystem_error& error)
        {
            throw std::runtime_error{"cannot use " + error.path1().filename().string()};
        }
    });
    std::string what;
    try
    {
        strict.file_size(missing);
    }
    catch (const std::runtime_error& error)
    {
        what = error.what();
    }
    check(what == "cannot use missing.txt", "an exception from on_error leaves the call");
}

// write, read, try_read, and content_filter on a real directory
//
void bytes()
{
    temp_dir_t dir;
    const auto file = dir.path / "notes.txt";

    // the filter shifts every byte up on the way to disk and down on the way back
    const auto filesystem = vfs::make_filesystem({
        .content_filter = [](
            const vfs::path_t&,
            const vfs::bytes_t& bytes,
            const vfs::content_filter_direction_et direction
        )
        {
            const auto shift = direction == vfs::content_filter_direction_et::WRITE ? 1 : -1;
            auto result = bytes;
            std::ranges::transform(result, result.begin(), [shift](const char c)
            {
                return static_cast<char>(c + shift);
            });
            return result;
        }
    });

    filesystem.write({
        .path = file,
        .bytes = "HAL"
    });
    check(filesystem.read(file) == "HAL", "read gives back what write was given");
    check(filesystem.try_read(file) == vfs::bytes_ot{"HAL"}, "try_read goes through the filter too");

    // the plain filesystem sees the bytes that are on disk
    const auto plain = vfs::make_filesystem({});
    check(plain.read(file) == "IBM", "the file holds the filtered bytes");
    check(!filesystem.try_read(dir.path / "none.txt").has_value(), "try_read of a missing file is empty, the filter is not called");

    // a hand-made filesystem_t: apply_content_filter wraps its read and write
    vfs::bytes_t on_disk = "one\r\ntwo\r\n";
    vfs::filesystem_t fake;
    fake.read = [&](const vfs::path_t&)
    {
        return on_disk;
    };
    fake.write = [&](const vfs::write_t& args)
    {
        on_disk = args.bytes;
    };
    fake.content_filter = [](
        const vfs::path_t&,
        const vfs::bytes_t& bytes,
        const vfs::content_filter_direction_et direction
    )
    {
        if (direction == vfs::content_filter_direction_et::WRITE)
        {
            return bytes;
        }
        auto text = bytes;
        std::erase(text, '\r');
        return text;
    };
    vfs::apply_content_filter(fake);
    check(fake.read("notes.txt") == "one\ntwo\n", "a filter on READ drops the carriage returns");
    fake.write({
        .path = "notes.txt",
        .bytes = "three\n"
    });
    check(on_disk == "three\n", "a filter that returns the bytes on WRITE leaves them as they are");
}

}// namespace

int main()
{
    fakes();
    queries();
    errors();
    bytes();

    if (failures != 0)
    {
        std::cout << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all checks passed\n";
    return 0;
}
