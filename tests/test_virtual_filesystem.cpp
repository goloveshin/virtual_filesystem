// doctest
#include <doctest/doctest.h>

// virtual_filesystem
#include "virtual_filesystem/filesystem.hpp"

// std
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>


namespace virtual_filesystem
{

namespace
{

// a directory removed when the test ends
//
struct temp_dir_t
{
    path_t path;

    temp_dir_t()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        static int counter = 0;
        path = fs::temp_directory_path() / (
            "virtual_filesystem_" + std::to_string(stamp) + "_" + std::to_string(++counter)
        );
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

// restores the process current path
//
struct cwd_guard_t
{
    path_t saved = fs::current_path();

    cwd_guard_t() = default;

    ~cwd_guard_t()
    {
        fs::current_path(saved);
    }

    cwd_guard_t(const cwd_guard_t&) = delete;
    cwd_guard_t& operator=(const cwd_guard_t&) = delete;
};

// writes bytes. fails the test when the file does not open
//
void write_file(
    const path_t& path,
    const std::string& bytes
)
{
    std::ofstream out{path, std::ios::binary};
    REQUIRE(out.is_open());
    out << bytes;
    REQUIRE(out.good());
}

// the entry whose path matches, or nullptr
//
const entry_t* find_entry(
    const entries_t& entries,
    const path_t& path
)
{
    const auto wanted = path.lexically_normal();
    for (const auto& entry : entries)
    {
        if (entry.path.lexically_normal() == wanted)
        {
            return &entry;
        }
    }
    return nullptr;
}

// status and symlink_status both answer this type
//
filesystem_t status_of(const fs::file_type type)
{
    filesystem_t filesystem;
    const auto answer = [type](const path_t&)
    {
        return fs::file_status{type};
    };
    filesystem.status = answer;
    filesystem.symlink_status = answer;
    return filesystem;
}

// on_error records the failure and returns, so the call yields an empty result
//
filesystem_t filesystem_that_records(
    bool& failed,
    std::string& what
)
{
    return make_filesystem({
        .on_error = [&](const fs::filesystem_error& error)
        {
            failed = true;
            what = error.what();
        }
    });
}

}// namespace

TEST_SUITE("virtual_filesystem/queries")
{

// exists and the is_* queries read status().type().
// is_symlink reads symlink_status(). a symlink type is not is_other
//
TEST_CASE("type queries read status, is_symlink reads symlink_status")
{
    const auto regular = status_of(fs::file_type::regular);
    CHECK(exists(regular, "p"));
    CHECK(is_regular_file(regular, "p"));
    CHECK_FALSE(is_directory(regular, "p"));
    CHECK_FALSE(is_block_file(regular, "p"));
    CHECK_FALSE(is_character_file(regular, "p"));
    CHECK_FALSE(is_fifo(regular, "p"));
    CHECK_FALSE(is_socket(regular, "p"));
    CHECK_FALSE(is_other(regular, "p"));
    CHECK_FALSE(is_symlink(regular, "p"));

    CHECK(is_directory(status_of(fs::file_type::directory), "p"));
    CHECK(is_block_file(status_of(fs::file_type::block), "p"));
    CHECK(is_other(status_of(fs::file_type::block), "p"));
    CHECK(is_character_file(status_of(fs::file_type::character), "p"));
    CHECK(is_other(status_of(fs::file_type::character), "p"));
    CHECK(is_fifo(status_of(fs::file_type::fifo), "p"));
    CHECK(is_other(status_of(fs::file_type::fifo), "p"));
    CHECK(is_socket(status_of(fs::file_type::socket), "p"));
    CHECK(is_other(status_of(fs::file_type::socket), "p"));
    CHECK(is_other(status_of(fs::file_type::unknown), "p"));
    CHECK(is_other(status_of(fs::file_type::junction), "p"));

    CHECK_FALSE(exists(status_of(fs::file_type::not_found), "p"));
    CHECK_FALSE(exists(status_of(fs::file_type::none), "p"));
    CHECK_FALSE(is_other(status_of(fs::file_type::not_found), "p"));
    CHECK_FALSE(is_other(status_of(fs::file_type::none), "p"));
    CHECK_FALSE(is_other(status_of(fs::file_type::symlink), "p"));
    CHECK_FALSE(is_other(status_of(fs::file_type::directory), "p"));
    CHECK_FALSE(is_other(status_of(fs::file_type::regular), "p"));

    filesystem_t followed;
    followed.status = [](const path_t&)
    {
        return fs::file_status{fs::file_type::regular};
    };
    followed.symlink_status = [](const path_t&)
    {
        return fs::file_status{fs::file_type::symlink};
    };
    CHECK(is_symlink(followed, "p"));
    CHECK(is_regular_file(followed, "p"));
    CHECK(exists(followed, "p"));
}

// a directory is empty when list returns no entries.
// a file is empty when file_size is 0. list is not called for a file
//
TEST_CASE("is_empty lists a directory and sizes a file")
{
    filesystem_t filesystem;
    filesystem.status = [](const path_t&)
    {
        return fs::file_status{fs::file_type::directory};
    };
    list_t listed;
    bool sized = false;
    filesystem.list = [&](const list_t& args)
    {
        listed = args;
        return entries_t{};
    };
    filesystem.file_size = [&](const path_t&)
    {
        sized = true;
        return std::uintmax_t{1};
    };

    CHECK(is_empty(filesystem, "dir"));
    CHECK(listed.root == "dir");
    CHECK_FALSE(listed.options.recursive);
    CHECK_FALSE(listed.options.follow_directory_symlink);
    CHECK_FALSE(listed.options.skip_permission_denied);
    CHECK_FALSE(sized);

    filesystem.list = [&](const list_t&)
    {
        return entries_t{entry_t{}};
    };
    CHECK_FALSE(is_empty(filesystem, "dir"));

    filesystem.status = [](const path_t&)
    {
        return fs::file_status{fs::file_type::regular};
    };
    bool listed_file = false;
    filesystem.list = [&](const list_t&)
    {
        listed_file = true;
        return entries_t{};
    };
    filesystem.file_size = [](const path_t&)
    {
        return std::uintmax_t{0};
    };
    CHECK(is_empty(filesystem, "file"));
    CHECK_FALSE(listed_file);

    filesystem.file_size = [](const path_t&)
    {
        return std::uintmax_t{4};
    };
    CHECK_FALSE(is_empty(filesystem, "file"));
}

// both paths are weakly_canonical first.
// relative is the lexical relative path.
// proximate is that path, or the original when one path is absolute and the other is not
//
TEST_CASE("relative and proximate canonicalize, then use the path")
{
    filesystem_t filesystem;
    filesystem.weakly_canonical = [](const path_t& path)
    {
        return path;
    };

    const auto file = path_t{"a"} / "b" / "c";
    const auto base = path_t{"a"};
    CHECK(relative(filesystem, file, base) == path_t{"b"} / "c");
    CHECK(proximate(filesystem, file, base) == path_t{"b"} / "c");

    const auto absolute_base = fs::current_path();
    CHECK(relative(filesystem, file, absolute_base).empty());
    CHECK(proximate(filesystem, file, absolute_base) == file);
}

// the free functions do not catch. filesystem_error from a member leaves
//
TEST_CASE("a query lets filesystem_error leave")
{
    filesystem_t filesystem;
    filesystem.status = [](const path_t&) -> fs::file_status
    {
        throw fs::filesystem_error{
            "status",
            "missing",
            std::make_error_code(std::errc::no_such_file_or_directory)
        };
    };

    CHECK_THROWS_AS(exists(filesystem, "missing"), fs::filesystem_error);
}

// content_filter wraps the current try_read, read, and write.
// an empty filter leaves those functions as they are.
// try_read skips the filter when it has no bytes
//
TEST_CASE("content_filter wraps read and write")
{
    bytes_t written;
    filesystem_t filesystem;
    filesystem.try_read = [](const path_t&) -> bytes_ot
    {
        return bytes_t{"raw"};
    };
    filesystem.read = [](const path_t&)
    {
        return bytes_t{"raw"};
    };
    filesystem.write = [&](const write_t& args)
    {
        written = args.bytes;
    };
    filesystem.content_filter = [](
        const path_t&,
        const bytes_t& bytes,
        const content_filter_direction_et
    )
    {
        return bytes + "x";
    };

    apply_content_filter(filesystem);
    CHECK(filesystem.read("p") == "rawx");
    CHECK(filesystem.try_read("p") == bytes_ot{"rawx"});
    filesystem.write({
        .path = "p",
        .bytes = "in"
    });
    CHECK(written == "inx");

    filesystem_t plain;
    plain.read = [](const path_t&)
    {
        return bytes_t{"raw"};
    };
    apply_content_filter(plain);
    CHECK(plain.read("p") == "raw");

    filesystem_t missing;
    missing.try_read = [](const path_t&) -> bytes_ot
    {
        return {};
    };
    missing.content_filter = [](
        const path_t&,
        const bytes_t&,
        const content_filter_direction_et
    ) -> bytes_t
    {
        return "filtered";
    };
    apply_content_filter(missing);
    CHECK_FALSE(missing.try_read("p").has_value());
}

}

TEST_SUITE("virtual_filesystem/std_filesystem")
{

// create_directories makes the missing parents.
// list returns the entries under root. recursive adds the nested ones.
// a second create of the same directory returns false and is not an error
//
TEST_CASE("directories, files, and list")
{
    temp_dir_t dir;
    bool failed = false;
    std::string what;
    const auto filesystem = filesystem_that_records(failed, what);

    const auto nested = dir.path / "sub";
    const auto file = dir.path / "a.txt";
    const auto inner = nested / "b.txt";
    CHECK(filesystem.create_directories(nested));
    CHECK_FALSE(filesystem.create_directories(nested));
    CHECK_FALSE(filesystem.create_directory(nested));
    write_file(file, "abcd");
    write_file(inner, "ef");

    CHECK(exists(filesystem, file));
    CHECK(is_regular_file(filesystem, file));
    CHECK(is_directory(filesystem, nested));
    CHECK_FALSE(is_symlink(filesystem, file));
    CHECK_FALSE(exists(filesystem, dir.path / "missing"));
    CHECK(filesystem.file_size(file) == 4);
    CHECK_FALSE(is_empty(filesystem, file));
    CHECK(filesystem.create_directory(dir.path / "empty"));
    CHECK_FALSE(filesystem.create_directory(dir.path / "empty"));
    CHECK(is_empty(filesystem, dir.path / "empty"));
    CHECK_FALSE(is_empty(filesystem, dir.path));

    const auto shallow = filesystem.list({
        .root = dir.path
    });
    CHECK(find_entry(shallow, file) != nullptr);
    CHECK(find_entry(shallow, nested) != nullptr);
    CHECK(find_entry(shallow, dir.path / "empty") != nullptr);
    CHECK(find_entry(shallow, inner) == nullptr);

    const auto* file_entry = find_entry(shallow, file);
    REQUIRE(file_entry != nullptr);
    CHECK(file_entry->status.type() == fs::file_type::regular);
    CHECK(file_entry->symlink_status.type() == fs::file_type::regular);

    const auto deep = filesystem.list({
        .root = dir.path,
        .options = {
            .recursive = true,
            .follow_directory_symlink = true,
            .skip_permission_denied = true
        }
    });
    CHECK(find_entry(deep, inner) != nullptr);
    CHECK(find_entry(deep, file) != nullptr);

    INFO(what);
    CHECK_FALSE(failed);
}

// copy, rename, and resize_file change the tree.
// remove of a missing path returns false.
// remove_all counts the directory and everything under it
//
TEST_CASE("copy, rename, resize, and remove")
{
    temp_dir_t dir;
    bool failed = false;
    std::string what;
    const auto filesystem = filesystem_that_records(failed, what);

    const auto source = dir.path / "source.txt";
    const auto copied = dir.path / "copied.txt";
    write_file(source, "abcd");
    filesystem.copy({
        .from = source,
        .to = copied
    });
    CHECK(filesystem.file_size(copied) == 4);

    const auto from_dir = dir.path / "from_dir";
    const auto to_dir = dir.path / "to_dir";
    CHECK(filesystem.create_directory(from_dir));
    write_file(from_dir / "d.txt", "dddd");
    filesystem.copy({
        .from = from_dir,
        .to = to_dir,
        .options = fs::copy_options::recursive
    });
    CHECK(filesystem.file_size(to_dir / "d.txt") == 4);

    const auto renamed = dir.path / "renamed.txt";
    filesystem.rename({
        .from = copied,
        .to = renamed
    });
    CHECK_FALSE(exists(filesystem, copied));
    CHECK(filesystem.file_size(renamed) == 4);

    filesystem.resize_file({
        .path = renamed,
        .new_size = 10
    });
    CHECK(filesystem.file_size(renamed) == 10);
    CHECK_FALSE(is_empty(filesystem, renamed));
    filesystem.resize_file({
        .path = renamed,
        .new_size = 0
    });
    CHECK(is_empty(filesystem, renamed));

    CHECK(filesystem.remove(renamed));
    CHECK_FALSE(filesystem.remove(renamed));

    const auto tree = dir.path / "tree";
    CHECK(filesystem.create_directories(tree / "nested"));
    write_file(tree / "nested" / "c.txt", "c");
    CHECK(filesystem.remove_all(tree) == 3);
    CHECK_FALSE(exists(filesystem, tree));
    CHECK(filesystem.remove_all(tree) == 0);

    INFO(what);
    CHECK_FALSE(failed);
}

// absolute and canonical name the same existing file.
// weakly_canonical of a missing path does not fail. canonical of it does.
// set_current_path of a missing directory does not change the process path
//
TEST_CASE("absolute, canonical, and current path")
{
    temp_dir_t dir;
    cwd_guard_t guard;
    bool failed = false;
    std::string what;
    const auto filesystem = filesystem_that_records(failed, what);

    const auto file = dir.path / "sub" / "file.txt";
    CHECK(filesystem.create_directories(file.parent_path()));
    write_file(file, "z");

    const auto absolute = filesystem.absolute(file);
    CHECK(absolute.is_absolute());
    CHECK(filesystem.equivalent({
        .path_1 = absolute,
        .path_2 = file
    }));

    const auto canonical = filesystem.canonical(file);
    CHECK(filesystem.equivalent({
        .path_1 = canonical,
        .path_2 = file
    }));
    CHECK(relative(filesystem, file, dir.path) == path_t{"sub"} / "file.txt");
    CHECK(proximate(filesystem, file, dir.path) == path_t{"sub"} / "file.txt");

    const auto temp = filesystem.temp_directory();
    CHECK(is_directory(filesystem, temp));

    const auto missing = dir.path / "no_such";
    const auto weak = filesystem.weakly_canonical(missing);
    CHECK(weak.is_absolute());
    CHECK(weak.filename() == "no_such");
    INFO(what);
    CHECK_FALSE(failed);

    bool canonical_failed = false;
    const auto failing = make_filesystem({
        .on_error = [&](const fs::filesystem_error&)
        {
            canonical_failed = true;
        }
    });
    CHECK(failing.canonical(missing).empty());
    CHECK(canonical_failed);

    const auto before = filesystem.current_path();
    bool set_failed = false;
    const auto setting = make_filesystem({
        .on_error = [&](const fs::filesystem_error&)
        {
            set_failed = true;
        }
    });
    setting.set_current_path(dir.path / "no_such_dir");
    CHECK(set_failed);
    CHECK(setting.equivalent({
        .path_1 = setting.current_path(),
        .path_2 = before
    }));

    set_failed = false;
    setting.set_current_path(dir.path);
    CHECK_FALSE(set_failed);
    CHECK(setting.equivalent({
        .path_1 = setting.current_path(),
        .path_2 = dir.path
    }));
}

// a hard link is the same file. two different files are not
//
TEST_CASE("hard link and equivalent")
{
    temp_dir_t dir;
    bool failed = false;
    std::string what;
    const auto filesystem = filesystem_that_records(failed, what);

    const auto file = dir.path / "file.txt";
    const auto other = dir.path / "other.txt";
    const auto link = dir.path / "link.txt";
    write_file(file, "abcd");
    write_file(other, "ef");

    CHECK(filesystem.equivalent({
        .path_1 = file,
        .path_2 = file
    }));
    CHECK_FALSE(filesystem.equivalent({
        .path_1 = file,
        .path_2 = other
    }));
    CHECK(filesystem.hard_link_count(file) == 1);

    filesystem.create_hard_link({
        .target = file,
        .link = link
    });
    CHECK(filesystem.hard_link_count(file) == 2);
    CHECK(filesystem.equivalent({
        .path_1 = file,
        .path_2 = link
    }));
    CHECK(filesystem.file_size(link) == 4);

    INFO(what);
    CHECK_FALSE(failed);
}

// creating a symlink needs privilege on Windows.
// when it is refused, on_error runs and the call does not throw.
// when it is created, read_symlink and is_symlink see it
//
TEST_CASE("symlink is created or the error callback runs")
{
    temp_dir_t dir;
    bool failed = false;
    const auto filesystem = make_filesystem({
        .on_error = [&](const fs::filesystem_error&)
        {
            failed = true;
        }
    });

    const auto target = dir.path / "target.txt";
    const auto link = dir.path / "file_link";
    write_file(target, "abc");
    filesystem.create_symlink({
        .target = target,
        .link = link
    });
    if (failed)
    {
        CHECK_FALSE(fs::exists(link));
    }
    else
    {
        CHECK(is_symlink(filesystem, link));
        CHECK(is_regular_file(filesystem, link));
        CHECK(filesystem.read_symlink(link).lexically_normal() == target.lexically_normal());
        CHECK(filesystem.symlink_status(link).type() == fs::file_type::symlink);
        CHECK(filesystem.status(link).type() == fs::file_type::regular);
    }

    failed = false;
    const auto target_dir = dir.path / "target_dir";
    const auto dir_link = dir.path / "dir_link";
    CHECK(filesystem.create_directory(target_dir));
    write_file(target_dir / "inside.txt", "in");
    filesystem.create_directory_symlink({
        .target = target_dir,
        .link = dir_link
    });
    if (failed)
    {
        CHECK_FALSE(fs::exists(dir_link));
        return;
    }

    CHECK(is_symlink(filesystem, dir_link));
    CHECK(is_directory(filesystem, dir_link));
    CHECK(filesystem.read_symlink(dir_link).lexically_normal() == target_dir.lexically_normal());

    const auto shallow = filesystem.list({
        .root = dir.path,
        .options = {
            .recursive = true
        }
    });
    CHECK(find_entry(shallow, dir_link / "inside.txt") == nullptr);

    const auto followed = filesystem.list({
        .root = dir.path,
        .options = {
            .recursive = true,
            .follow_directory_symlink = true
        }
    });
    CHECK(find_entry(followed, dir_link / "inside.txt") != nullptr);
}

// space reports the volume that holds the path.
// last_write_time stores the time it is given.
// permissions on a file is not an error
//
TEST_CASE("space, permissions, and last write time")
{
    temp_dir_t dir;
    bool failed = false;
    std::string what;
    const auto filesystem = filesystem_that_records(failed, what);

    const auto info = filesystem.space(dir.path);
    CHECK(info.capacity > 0);
    CHECK(info.free <= info.capacity);
    CHECK(info.available <= info.capacity);

    const auto file = dir.path / "timed.txt";
    write_file(file, "t");
    const auto time = fs::file_time_type::clock::now() - std::chrono::hours{2};
    filesystem.set_last_write_time({
        .path = file,
        .time = time
    });
    CHECK(filesystem.last_write_time(file) == time);

    filesystem.permissions({
        .path = file,
        .permissions = fs::perms::owner_all,
        .options = fs::perm_options::replace
    });
    CHECK(is_regular_file(filesystem, file));

    INFO(what);
    CHECK_FALSE(failed);
}

// the logger runs before the call.
// filesystem_error is passed to on_error.
// when on_error returns, the result is value-initialized.
// status of a missing path is not_found and does not call on_error.
// a missing file is not a directory, and the swallowed size is 0, so is_empty is true
//
TEST_CASE("filesystem_error is logged, then handled")
{
    temp_dir_t dir;
    const auto file = dir.path / "file.txt";
    write_file(file, "abcd");

    std::vector<log_t> logs;
    std::vector<std::string> events;
    path_t error_path;
    const auto filesystem = make_filesystem({
        .logger = [&](const log_t& log)
        {
            logs.push_back(log);
            events.push_back("log");
        },
        .on_error = [&](const fs::filesystem_error& error)
        {
            error_path = error.path1();
            events.push_back("error");
        }
    });

    logs.clear();
    events.clear();
    CHECK(filesystem.status(file).type() == fs::file_type::regular);
    REQUIRE(logs.size() == 1);
    CHECK(logs[0].operation == operation_et::STATUS);
    CHECK(logs[0].path == file);
    CHECK(logs[0].other.empty());
    CHECK(events == std::vector<std::string>{"log"});

    logs.clear();
    events.clear();
    const auto cwd = filesystem.current_path();
    CHECK_FALSE(cwd.empty());
    REQUIRE(logs.size() == 1);
    CHECK(logs[0].operation == operation_et::CURRENT_PATH);
    CHECK(logs[0].path.empty());
    CHECK(logs[0].other.empty());

    const auto renamed = dir.path / "renamed.txt";
    logs.clear();
    events.clear();
    filesystem.rename({
        .from = file,
        .to = renamed
    });
    REQUIRE(logs.size() == 1);
    CHECK(logs[0].operation == operation_et::RENAME);
    CHECK(logs[0].path == file);
    CHECK(logs[0].other == renamed);
    CHECK(events == std::vector<std::string>{"log"});

    const auto missing = dir.path / "missing.txt";
    logs.clear();
    events.clear();
    CHECK(filesystem.file_size(missing) == 0);
    REQUIRE(logs.size() == 1);
    CHECK(logs[0].operation == operation_et::FILE_SIZE);
    CHECK(logs[0].path == missing);
    CHECK(error_path == missing);
    CHECK(events == std::vector<std::string>{"log", "error"});

    logs.clear();
    events.clear();
    CHECK(filesystem.status(missing).type() == fs::file_type::not_found);
    CHECK(events == std::vector<std::string>{"log"});
    CHECK_FALSE(exists(filesystem, missing));
    CHECK(is_empty(filesystem, missing));

    events.clear();
    CHECK(filesystem.list({
        .root = renamed
    }).empty());
    CHECK(events == std::vector<std::string>{"log", "error"});

    events.clear();
    filesystem.resize_file({
        .path = missing,
        .new_size = 3
    });
    CHECK(events == std::vector<std::string>{"log", "error"});

    events.clear();
    CHECK(filesystem.read_symlink(renamed).empty());
    CHECK(events == std::vector<std::string>{"log", "error"});

    logs.clear();
    events.clear();
    CHECK(filesystem.read(renamed) == "abcd");
    REQUIRE(logs.size() == 1);
    CHECK(logs[0].operation == operation_et::READ);
    CHECK(logs[0].path == renamed);
    CHECK(events == std::vector<std::string>{"log"});

    const auto missing_read = dir.path / "no_bytes.txt";
    logs.clear();
    events.clear();
    CHECK_FALSE(filesystem.try_read(missing_read).has_value());
    REQUIRE(logs.size() == 1);
    CHECK(logs[0].operation == operation_et::TRY_READ);
    CHECK(events == std::vector<std::string>{"log"});

    logs.clear();
    events.clear();
    CHECK(filesystem.read(missing_read).empty());
    CHECK(events == std::vector<std::string>{"log", "error"});
}

// on_error may throw its own exception. that exception leaves the call
//
TEST_CASE("an exception from on_error leaves the call")
{
    temp_dir_t dir;
    const auto filesystem = make_filesystem({
        .on_error = [](const fs::filesystem_error&)
        {
            throw std::runtime_error{"from callback"};
        }
    });

    bool left = false;
    std::string what;
    try
    {
        filesystem.file_size(dir.path / "missing.txt");
    }
    catch (const std::runtime_error& error)
    {
        left = true;
        what = error.what();
    }

    CHECK(left);
    CHECK(what == "from callback");
}

// an empty logger and an empty on_error are not called.
// a filesystem_error still does not leave the call
//
TEST_CASE("empty callbacks skip logging and do not throw")
{
    temp_dir_t dir;
    const auto file = dir.path / "file.txt";
    write_file(file, "abcd");
    const auto filesystem = make_filesystem({});

    CHECK(filesystem.file_size(file) == 4);
    CHECK(filesystem.read(file) == "abcd");
    CHECK(filesystem.try_read(file) == bytes_ot{"abcd"});
    CHECK_NOTHROW(filesystem.file_size(dir.path / "missing.txt"));
    CHECK(filesystem.file_size(dir.path / "missing.txt") == 0);
    CHECK(filesystem.status(file).type() == fs::file_type::regular);
}

// write replaces the bytes. read and try_read return them.
// a missing file is empty from try_read, and an error from read.
// the filter sees the bytes on the way to disk and on the way back
//
TEST_CASE("try_read, read, write, and content_filter")
{
    temp_dir_t dir;
    bool failed = false;
    std::string what;
    const auto filesystem = filesystem_that_records(failed, what);
    const auto file = dir.path / "bytes.txt";

    filesystem.write({
        .path = file,
        .bytes = "abcd"
    });
    CHECK(filesystem.read(file) == "abcd");
    CHECK(filesystem.try_read(file) == bytes_ot{"abcd"});
    CHECK(filesystem.file_size(file) == 4);

    filesystem.write({
        .path = file,
        .bytes = ""
    });
    const auto empty = filesystem.try_read(file);
    REQUIRE(empty.has_value());
    CHECK(empty->empty());

    bool saw = false;
    const auto reporting = make_filesystem({
        .on_error = [&](const fs::filesystem_error&)
        {
            saw = true;
        }
    });
    const auto missing = dir.path / "missing.txt";
    CHECK_FALSE(reporting.try_read(missing).has_value());
    CHECK_FALSE(saw);
    CHECK(reporting.read(missing).empty());
    CHECK(saw);

    const auto filtered_path = dir.path / "filtered.txt";
    const auto filtered = make_filesystem({
        .content_filter = [](
            const path_t&,
            const bytes_t& bytes,
            const content_filter_direction_et direction
        )
        {
            if (direction == content_filter_direction_et::WRITE)
            {
                return bytes + ">w";
            }
            return bytes + ">r";
        }
    });
    filtered.write({
        .path = filtered_path,
        .bytes = "ab"
    });
    std::ifstream on_disk{filtered_path, std::ios::binary};
    const bytes_t stored{
        std::istreambuf_iterator<char>{on_disk},
        std::istreambuf_iterator<char>{}
    };
    CHECK(stored == "ab>w");
    CHECK(filtered.read(filtered_path) == "ab>w>r");
    CHECK(filtered.try_read(filtered_path) == bytes_ot{"ab>w>r"});

    INFO(what);
    CHECK_FALSE(failed);
}

}

}// namespace virtual_filesystem
