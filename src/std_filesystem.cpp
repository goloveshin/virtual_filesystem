// virtual_filesystem
#include "virtual_filesystem/filesystem.hpp"

// std
#include <fstream>
#include <iterator>
#include <memory>
#include <system_error>


namespace virtual_filesystem
{

namespace
{

struct callbacks_t
{
    logger_ft logger;
    error_ft on_error;
};

void log(
    const callbacks_t& callbacks,
    const operation_et operation,
    const path_t& path,
    const path_t& other
)
{
    if (callbacks.logger)
    {
        callbacks.logger({
            .operation = operation,
            .path = path,
            .other = other
        });
    }
}

void fail(const callbacks_t& callbacks, const fs::filesystem_error& error)
{
    if (callbacks.on_error)
    {
        callbacks.on_error(error);
    }
}

template <typename Result, typename Action>
Result call(
    const callbacks_t& callbacks,
    const operation_et operation,
    const path_t& path,
    const path_t& other,
    Action&& action
)
{
    log(callbacks, operation, path, other);
    try
    {
        return action();
    }
    catch (const fs::filesystem_error& error)
    {
        fail(callbacks, error);
        return Result{};
    }
}

template <typename Action>
void call_void(
    const callbacks_t& callbacks,
    const operation_et operation,
    const path_t& path,
    const path_t& other,
    Action&& action
)
{
    log(callbacks, operation, path, other);
    try
    {
        action();
    }
    catch (const fs::filesystem_error& error)
    {
        fail(callbacks, error);
    }
}

bytes_ot try_read_bytes(const path_t& path)
{
    std::ifstream in{path, std::ios::in | std::ios::binary};
    if (!in)
    {
        return {};
    }
    return bytes_t{
        std::istreambuf_iterator<char>{in},
        std::istreambuf_iterator<char>{}
    };
}

bytes_t read_bytes(const path_t& path)
{
    auto bytes = try_read_bytes(path);
    if (!bytes)
    {
        const auto type = fs::status(path).type();
        const auto missing = type == fs::file_type::not_found || type == fs::file_type::none;
        const auto code = missing
            ? std::make_error_code(std::errc::no_such_file_or_directory)
            : std::make_error_code(std::errc::io_error);
        throw fs::filesystem_error{"read", path, code};
    }
    return *bytes;
}

void write_bytes(const path_t& path, const bytes_t& bytes)
{
    std::ofstream out{path, std::ios::out | std::ios::binary | std::ios::trunc};
    if (!out)
    {
        throw fs::filesystem_error{"write", path, std::make_error_code(std::errc::io_error)};
    }
    if (!bytes.empty())
    {
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    if (!out)
    {
        throw fs::filesystem_error{"write", path, std::make_error_code(std::errc::io_error)};
    }
}

fs::directory_options directory_options(const list_options_t& options)
{
    auto result = fs::directory_options::none;
    if (options.follow_directory_symlink)
    {
        result |= fs::directory_options::follow_directory_symlink;
    }
    if (options.skip_permission_denied)
    {
        result |= fs::directory_options::skip_permission_denied;
    }
    return result;
}

}// namespace

filesystem_t make_filesystem(const make_filesystem_t& args)
{
    const auto held = callbacks_t{
        .logger = args.logger,
        .on_error = args.on_error
    };
    const auto callbacks = std::make_shared<callbacks_t>(held);

    auto filesystem = filesystem_t{
        .status = [callbacks](const path_t& path)
        {
            return call<fs::file_status>(*callbacks, operation_et::STATUS, path, {}, [&]
            {
                return fs::status(path);
            });
        },
        .symlink_status = [callbacks](const path_t& path)
        {
            return call<fs::file_status>(*callbacks, operation_et::SYMLINK_STATUS, path, {}, [&]
            {
                return fs::symlink_status(path);
            });
        },
        .file_size = [callbacks](const path_t& path)
        {
            return call<std::uintmax_t>(*callbacks, operation_et::FILE_SIZE, path, {}, [&]
            {
                return fs::file_size(path);
            });
        },
        .hard_link_count = [callbacks](const path_t& path)
        {
            return call<std::uintmax_t>(*callbacks, operation_et::HARD_LINK_COUNT, path, {}, [&]
            {
                return fs::hard_link_count(path);
            });
        },
        .last_write_time = [callbacks](const path_t& path)
        {
            return call<fs::file_time_type>(*callbacks, operation_et::LAST_WRITE_TIME, path, {}, [&]
            {
                return fs::last_write_time(path);
            });
        },
        .space = [callbacks](const path_t& path)
        {
            return call<fs::space_info>(*callbacks, operation_et::SPACE, path, {}, [&]
            {
                return fs::space(path);
            });
        },
        .equivalent = [callbacks](const equivalent_t& args)
        {
            return call<bool>(*callbacks, operation_et::EQUIVALENT, args.path_1, args.path_2, [&]
            {
                return fs::equivalent(args.path_1, args.path_2);
            });
        },
        .read_symlink = [callbacks](const path_t& path)
        {
            return call<path_t>(*callbacks, operation_et::READ_SYMLINK, path, {}, [&]
            {
                return fs::read_symlink(path);
            });
        },
        .current_path = [callbacks]()
        {
            return call<path_t>(*callbacks, operation_et::CURRENT_PATH, {}, {}, []
            {
                return fs::current_path();
            });
        },
        .set_current_path = [callbacks](const path_t& path)
        {
            call_void(*callbacks, operation_et::SET_CURRENT_PATH, path, {}, [&]
            {
                fs::current_path(path);
            });
        },
        .absolute = [callbacks](const path_t& path)
        {
            return call<path_t>(*callbacks, operation_et::ABSOLUTE_PATH, path, {}, [&]
            {
                return fs::absolute(path);
            });
        },
        .canonical = [callbacks](const path_t& path)
        {
            return call<path_t>(*callbacks, operation_et::CANONICAL, path, {}, [&]
            {
                return fs::canonical(path);
            });
        },
        .weakly_canonical = [callbacks](const path_t& path)
        {
            return call<path_t>(*callbacks, operation_et::WEAKLY_CANONICAL, path, {}, [&]
            {
                return fs::weakly_canonical(path);
            });
        },
        .temp_directory = [callbacks]()
        {
            return call<path_t>(*callbacks, operation_et::TEMP_DIRECTORY, {}, {}, []
            {
                return fs::temp_directory_path();
            });
        },
        .create_directory = [callbacks](const path_t& path)
        {
            return call<bool>(*callbacks, operation_et::CREATE_DIRECTORY, path, {}, [&]
            {
                return fs::create_directory(path);
            });
        },
        .create_directories = [callbacks](const path_t& path)
        {
            return call<bool>(*callbacks, operation_et::CREATE_DIRECTORIES, path, {}, [&]
            {
                return fs::create_directories(path);
            });
        },
        .remove = [callbacks](const path_t& path)
        {
            return call<bool>(*callbacks, operation_et::REMOVE, path, {}, [&]
            {
                return fs::remove(path);
            });
        },
        .remove_all = [callbacks](const path_t& path)
        {
            return call<std::uintmax_t>(*callbacks, operation_et::REMOVE_ALL, path, {}, [&]
            {
                return fs::remove_all(path);
            });
        },
        .rename = [callbacks](const rename_t& args)
        {
            call_void(*callbacks, operation_et::RENAME, args.from, args.to, [&]
            {
                fs::rename(args.from, args.to);
            });
        },
        .resize_file = [callbacks](const resize_file_t& args)
        {
            call_void(*callbacks, operation_et::RESIZE_FILE, args.path, {}, [&]
            {
                fs::resize_file(args.path, args.new_size);
            });
        },
        .copy = [callbacks](const copy_t& args)
        {
            call_void(*callbacks, operation_et::COPY, args.from, args.to, [&]
            {
                fs::copy(args.from, args.to, args.options);
            });
        },
        .create_symlink = [callbacks](const create_symlink_t& args)
        {
            call_void(*callbacks, operation_et::CREATE_SYMLINK, args.target, args.link, [&]
            {
                fs::create_symlink(args.target, args.link);
            });
        },
        .create_directory_symlink = [callbacks](const create_symlink_t& args)
        {
            call_void(*callbacks, operation_et::CREATE_DIRECTORY_SYMLINK, args.target, args.link, [&]
            {
                fs::create_directory_symlink(args.target, args.link);
            });
        },
        .create_hard_link = [callbacks](const create_hard_link_t& args)
        {
            call_void(*callbacks, operation_et::CREATE_HARD_LINK, args.target, args.link, [&]
            {
                fs::create_hard_link(args.target, args.link);
            });
        },
        .permissions = [callbacks](const permissions_t& args)
        {
            call_void(*callbacks, operation_et::PERMISSIONS, args.path, {}, [&]
            {
                fs::permissions(args.path, args.permissions, args.options);
            });
        },
        .set_last_write_time = [callbacks](const set_last_write_time_t& args)
        {
            call_void(*callbacks, operation_et::SET_LAST_WRITE_TIME, args.path, {}, [&]
            {
                fs::last_write_time(args.path, args.time);
            });
        },
        .list = [callbacks](const list_t& args)
        {
            return call<entries_t>(*callbacks, operation_et::LIST, args.root, {}, [&]
            {
                entries_t entries;
                const auto options = directory_options(args.options);
                const auto push = [&](const fs::directory_entry& entry)
                {
                    entries.push_back({
                        .path = entry.path(),
                        .status = entry.status(),
                        .symlink_status = entry.symlink_status()
                    });
                };
                if (args.options.recursive)
                {
                    for (const auto& entry : fs::recursive_directory_iterator(args.root, options))
                    {
                        push(entry);
                    }
                }
                else
                {
                    for (const auto& entry : fs::directory_iterator(args.root, options))
                    {
                        push(entry);
                    }
                }
                return entries;
            });
        },
        .try_read = [callbacks](const path_t& path) -> bytes_ot
        {
            return call<bytes_ot>(*callbacks, operation_et::TRY_READ, path, {}, [&]
            {
                return try_read_bytes(path);
            });
        },
        .read = [callbacks](const path_t& path)
        {
            return call<bytes_t>(*callbacks, operation_et::READ, path, {}, [&]
            {
                return read_bytes(path);
            });
        },
        .write = [callbacks](const write_t& args)
        {
            call_void(*callbacks, operation_et::WRITE, args.path, {}, [&]
            {
                write_bytes(args.path, args.bytes);
            });
        },
        .content_filter = args.content_filter
    };
    apply_content_filter(filesystem);
    return filesystem;
}

}// namespace virtual_filesystem
