#pragma once
#include "engine.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

enum class FileSource { FILE, BINARY };

struct FileHandle {
    // RAII: copying the handle keeps the file allocation alive.
    std::shared_ptr<const std::vector<uint8_t>> owner;
    const uint8_t* data = nullptr;
    uint64_t size = 0;
    uint64_t offset = 0; // offset inside the owned allocation

    explicit operator bool() const noexcept { return owner != nullptr; }
};

struct FileEntry {
    FileSource source = FileSource::FILE;
    std::filesystem::path diskPath;
    std::shared_ptr<const std::vector<uint8_t>> binary; // embedded/pack bytes
    std::weak_ptr<const std::vector<uint8_t>> cached;   // loose-file cache
    uint64_t offset = 0;
    uint64_t size = 0;
};

class FileSystem : public EngineModule {
public:
    using EngineModule::EngineModule;

    // Last binding wins on duplicate virtual paths.
    void BindFolder(const std::string& path, const std::string& root = "");
    void BindBinary(const uint8_t* data, uint64_t size, const std::string& path);
    void BindPack(const std::string& path, const std::string& root = "");
    void BindBinaryPack(const uint8_t* data, uint64_t size, const std::string& root = "");

    FileHandle Read(const std::string& path);
    void SavePack(const std::string& path); // snapshot of the currently visible files
    void Clear() { files.clear(); }

private:
    void BindPackStorage(std::shared_ptr<const std::vector<uint8_t>> bytes,
                         const std::string& root);
    // Resolve a physical path relative to the executable directory.
    // Absolute paths remain absolute.
    static std::filesystem::path PhysicalPath(const std::string& path);
    static std::string VirtualPath(const std::string& path);
    static std::shared_ptr<const std::vector<uint8_t>> LoadFile(const std::filesystem::path& path);

    std::unordered_map<std::string, FileEntry> files;
};
