#ifndef DESK_STORAGE_H
#define DESK_STORAGE_H
#include "model.h"
#include <filesystem>
namespace desk {
namespace fs = std::filesystem;
// Owns the on-disk session lock. Copying would give two owners of the same lock.
class PathLock {
    fs::path directory_;
public:
    explicit PathLock(fs::path directory);
    ~PathLock();
    PathLock(const PathLock&) = delete;
    PathLock& operator=(const PathLock&) = delete;
    const fs::path& path() const { return directory_; }
};
class Storage {
    fs::path file_, backup_;
    PathLock lock_;
public:
    explicit Storage(const fs::path& file);
    // Read and check a snapshot, or create an empty in-memory state on the given day.
    State load(Date initialDay) const;
    // The last successful primary remains in place until a complete draft can replace it.
    void save(const State& state) const;
};
// Write a new CSV file. An existing path is reported as an error.
void writeCsv(const fs::path& destination, const std::string& contents);
}
#endif
