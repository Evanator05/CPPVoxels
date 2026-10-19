#include "filesystem.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
constexpr char kMagic[8] = {'V','F','S','1','P','A','C','K'};

void Append32(std::vector<uint8_t>& out, uint32_t n) {
    for (int i = 0; i < 4; ++i) out.push_back(uint8_t(n >> (8 * i)));
}
void Append64(std::vector<uint8_t>& out, uint64_t n) {
    for (int i = 0; i < 8; ++i) out.push_back(uint8_t(n >> (8 * i)));
}

class Reader {
public:
    explicit Reader(const std::vector<uint8_t>& bytes) : bytes(bytes) {}
    uint64_t Remaining() const { return bytes.size() - cursor; }
    uint64_t Position() const { return cursor; }
    void Skip(uint64_t count) {
        if (count > Remaining()) throw std::runtime_error("Truncated or corrupt VFS pack");
        cursor += static_cast<size_t>(count);
    }
    uint32_t U32() {
        if (Remaining() < 4) throw std::runtime_error("Truncated VFS pack");
        uint32_t n = 0;
        for (int i = 0; i < 4; ++i) n |= uint32_t(bytes[cursor++]) << (8 * i);
        return n;
    }
    uint64_t U64() {
        if (Remaining() < 8) throw std::runtime_error("Truncated VFS pack");
        uint64_t n = 0;
        for (int i = 0; i < 8; ++i) n |= uint64_t(bytes[cursor++]) << (8 * i);
        return n;
    }
    std::string String(uint32_t length) {
        if (length > Remaining()) throw std::runtime_error("Truncated VFS pack path");
        auto ptr = reinterpret_cast<const char*>(bytes.data() + cursor);
        std::string result(ptr, length);
        Skip(length);
        return result;
    }
private:
    const std::vector<uint8_t>& bytes;
    size_t cursor = 0;
};
} // namespace

std::filesystem::path FileSystem::PhysicalPath(const std::string& path) {
    const std::filesystem::path requested(path);

    // Explicit absolute paths may still be used for external mounts (e.g. mods).
    if (requested.is_absolute())
        return requested.lexically_normal();

    const char* basePath = SDL_GetBasePath();
    if (!basePath)
        throw std::runtime_error(std::string("Cannot get executable directory: ") + SDL_GetError());

    // SDL_GetBasePath returns UTF-8. char8_t preserves it on Windows.
    const std::filesystem::path executableDir(reinterpret_cast<const char8_t*>(basePath));
    return (executableDir / requested).lexically_normal();
}

std::string FileSystem::VirtualPath(const std::string& path) {
    std::string value = path;
    std::replace(value.begin(), value.end(), '\\', '/');
    std::filesystem::path p(value);
    if (p.is_absolute() || p.has_root_name())
        throw std::invalid_argument("Virtual paths must be relative: " + path);
    p = p.lexically_normal();
    std::string result = p.generic_string();
    if (result == ".") result.clear();
    if (result == ".." || result.rfind("../", 0) == 0)
        throw std::invalid_argument("Virtual paths cannot escape the root: " + path);
    return result;
}

std::shared_ptr<const std::vector<uint8_t>> FileSystem::LoadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("Cannot open file: " + path.string());
    const auto end = in.tellg();
    if (end < 0 || static_cast<uint64_t>(end) > std::numeric_limits<size_t>::max())
        throw std::runtime_error("File too large: " + path.string());
    auto data = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(end));
    in.seekg(0);
    if (!data->empty() && !in.read(reinterpret_cast<char*>(data->data()),
                                   static_cast<std::streamsize>(data->size())))
        throw std::runtime_error("Cannot read file: " + path.string());
    return data;
}


void FileSystem::BindFolder(const std::string& path, const std::string& root) {
    const std::filesystem::path base = PhysicalPath(path);

    if (!std::filesystem::is_directory(base))
        throw std::runtime_error("Not a directory: " + base.string());

    // Preserve the mounted folder name in the virtual filesystem.
    const std::string prefix = VirtualPath(
        (std::filesystem::path(root) / base.filename()).generic_string()
    );

    for (const auto& item : std::filesystem::recursive_directory_iterator(base)) {
        if (!item.is_regular_file())
            continue;

        std::string name = VirtualPath(
            (std::filesystem::path(prefix) /
             item.path().lexically_relative(base)).generic_string()
        );

        FileEntry entry;
        entry.source = FileSource::FILE;
        entry.diskPath = item.path();
        entry.size = item.file_size();

        files[name] = std::move(entry);
    }
}

void FileSystem::BindBinary(const uint8_t* data, uint64_t size, const std::string& path) {
    auto name = VirtualPath(path);
    if (name.empty()) throw std::invalid_argument("File path cannot be empty");
    if ((!data && size) || size > std::numeric_limits<size_t>::max())
        throw std::invalid_argument("Invalid binary buffer");
    auto bytes = std::make_shared<std::vector<uint8_t>>();
    if (size) bytes->assign(data, data + size); // safe owned copy
    FileEntry entry;
    entry.source = FileSource::BINARY;
    entry.binary = std::move(bytes);
    entry.size = size;
    files[name] = std::move(entry);
}

void FileSystem::BindPack(const std::string& path, const std::string& root) {
    BindPackStorage(LoadFile(PhysicalPath(path)), root);
}

void FileSystem::BindBinaryPack(const uint8_t* data, uint64_t size, const std::string& root) {
    if ((!data && size) || size > std::numeric_limits<size_t>::max())
        throw std::invalid_argument("Invalid pack buffer");
    auto bytes = std::make_shared<std::vector<uint8_t>>();
    if (size) bytes->assign(data, data + size);
    BindPackStorage(std::move(bytes), root);
}

void FileSystem::BindPackStorage(std::shared_ptr<const std::vector<uint8_t>> bytes, const std::string& root) {
    Reader reader(*bytes);
    if (reader.Remaining() < 12 || std::memcmp(bytes->data(), kMagic, 8) != 0)
        throw std::runtime_error("Unknown pack format");
    reader.Skip(8);
    uint32_t count = reader.U32();
    const std::string prefix = VirtualPath(root);
    std::unordered_map<std::string, FileEntry> pending;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t nameLength = reader.U32();
        uint64_t dataLength = reader.U64();
        std::string name = VirtualPath(reader.String(nameLength));
        if (name.empty()) throw std::runtime_error("Pack has empty path");
        uint64_t offset = reader.Position();
        reader.Skip(dataLength);
        FileEntry entry;
        entry.source = FileSource::BINARY;
        entry.binary = bytes;
        entry.offset = offset;
        entry.size = dataLength;
        auto fullName = VirtualPath((std::filesystem::path(prefix) / name).generic_string());
        if (!pending.emplace(fullName, std::move(entry)).second)
            throw std::runtime_error("Duplicate path inside pack: " + fullName);
    }
    if (reader.Remaining() != 0) throw std::runtime_error("Trailing bytes in VFS pack");
    for (auto& [name, entry] : pending) files[name] = std::move(entry);
}

FileHandle FileSystem::Read(const std::string& path) {
    auto it = files.find(VirtualPath(path));
    if (it == files.end()) return {};
    auto& entry = it->second;
    std::shared_ptr<const std::vector<uint8_t>> owner;
    if (entry.source == FileSource::FILE) {
        owner = entry.cached.lock();
        if (!owner) {
            owner = LoadFile(entry.diskPath);
            entry.cached = owner;
            entry.size = owner->size();
        }
    } else {
        owner = entry.binary;
    }
    if (!owner || entry.offset > owner->size() || entry.size > owner->size() - entry.offset)
        throw std::runtime_error("Invalid VFS entry for: " + path);
    FileHandle handle;
    handle.owner = std::move(owner);
    handle.offset = entry.offset;
    handle.size = entry.size;
    handle.data = handle.size ? handle.owner->data() + static_cast<size_t>(handle.offset) : nullptr;
    return handle;
}

void FileSystem::SavePack(const std::string& path) {
    // Sorted paths yield deterministic pack ordering.
    std::vector<std::string> names;
    names.reserve(files.size());
    for (const auto& [name, _] : files) names.push_back(name);
    std::sort(names.begin(), names.end());
    if (names.size() > UINT32_MAX) throw std::runtime_error("Too many files to pack");

    std::vector<uint8_t> output(std::begin(kMagic), std::end(kMagic));
    Append32(output, static_cast<uint32_t>(names.size()));
    for (const auto& name : names) {
        if (name.size() > UINT32_MAX) throw std::runtime_error("Pack path too long");
        auto handle = Read(name);
        Append32(output, static_cast<uint32_t>(name.size()));
        Append64(output, handle.size);
        output.insert(output.end(), name.begin(), name.end());
        if (handle.size) output.insert(output.end(), handle.data, handle.data + handle.size);
    }
    const std::filesystem::path destination = PhysicalPath(path);
    std::ofstream out(destination, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot create pack: " + destination.string());
    if (output.size() > static_cast<size_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::runtime_error("Pack too large to write");
    out.write(reinterpret_cast<const char*>(output.data()),
              static_cast<std::streamsize>(output.size()));
    if (!out) throw std::runtime_error("Failed writing pack: " + destination.string());
}
