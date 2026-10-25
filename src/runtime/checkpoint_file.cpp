#include "openrepublic/runtime/checkpoint_file.h"

#include "openrepublic/economy/checkpoint.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace openrepublic::runtime {
namespace {
std::atomic<unsigned long long> nextTemp{0};

std::filesystem::path TempPath(const std::filesystem::path &path) {
    auto temp = path;
    temp += std::filesystem::path(".tmp.openrepublic.");
    temp += std::filesystem::path(std::to_string(nextTemp.fetch_add(1, std::memory_order_relaxed)));
    return temp;
}

std::string SystemError(const char *operation) {
#ifdef _WIN32
    return std::string(operation) + " failed (Windows error " + std::to_string(GetLastError()) + ")";
#else
    return std::string(operation) + " failed: " + std::system_category().message(errno);
#endif
}
}

namespace {
bool PublishBytes(const std::filesystem::path &path, std::string_view checkpoint,
                  std::string &error, bool replace, std::size_t maximumBytes) {
    error.clear();
    if (path.empty()) { error = "checkpoint path is empty"; return false; }
    if (checkpoint.empty()) { error = "checkpoint payload is empty"; return false; }
    if (checkpoint.size() > maximumBytes) { error = "file payload exceeds the size limit"; return false; }
    const auto temp = TempPath(path);
#ifdef _WIN32
    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = SystemError("create temporary checkpoint"); return false; }
    bool ok = true;
    std::size_t offset = 0;
    while (offset < checkpoint.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(checkpoint.size() - offset, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(file, checkpoint.data() + offset, chunk, &written, nullptr) || written == 0) { ok = false; break; }
        offset += written;
    }
    if (ok && !FlushFileBuffers(file)) ok = false;
    const DWORD writeError = GetLastError();
    CloseHandle(file);
    if (!ok) { SetLastError(writeError); error = SystemError("write checkpoint"); DeleteFileW(temp.c_str()); return false; }
    const DWORD publishFlags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
    if (!MoveFileExW(temp.c_str(), path.c_str(), publishFlags)) {
        error = SystemError("publish checkpoint without replacement"); DeleteFileW(temp.c_str()); return false;
    }
#else
    const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) { error = SystemError("create temporary checkpoint"); return false; }
    bool ok = true;
    std::size_t offset = 0;
    while (offset < checkpoint.size()) {
        const auto written = ::write(fd, checkpoint.data() + offset, checkpoint.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { ok = false; break; }
        offset += static_cast<std::size_t>(written);
    }
    if (ok && ::fsync(fd) != 0) ok = false;
    const int writeError = errno;
    ::close(fd);
    if (!ok) { errno = writeError; error = SystemError("write checkpoint"); ::unlink(temp.c_str()); return false; }
    if ((replace ? ::rename(temp.c_str(), path.c_str()) : ::link(temp.c_str(), path.c_str())) != 0) {
        error = SystemError("publish checkpoint without replacement"); ::unlink(temp.c_str()); return false;
    }
    if (!replace) ::unlink(temp.c_str());
    const auto parent = path.parent_path().empty() ? std::filesystem::path(".") : path.parent_path();
    const int dir = ::open(parent.c_str(), O_RDONLY);
    if (dir >= 0) { (void)::fsync(dir); ::close(dir); }
#endif
    return true;
}
}

bool WriteFileExclusive(const std::filesystem::path& path, std::string_view bytes,
                        std::size_t maximumBytes, std::string& error) {
    if (bytes.size() > maximumBytes) { error = "file payload exceeds the size limit"; return false; }
    return PublishBytes(path, bytes, error, false, maximumBytes);
}

bool ReplaceFileAtomically(const std::filesystem::path& path, std::string_view bytes,
                           std::size_t maximumBytes, std::string& error) {
    if (bytes.size() > maximumBytes) { error = "file payload exceeds the size limit"; return false; }
    return PublishBytes(path, bytes, error, true, maximumBytes);
}

bool WriteCheckpointFileExclusive(const std::filesystem::path &path,
                                  std::string_view checkpoint,
                                  std::string &error) {
    economy::RestoredCheckpoint restored;
    economy::ParseError parseError;
    if (!economy::RestoreCheckpoint(checkpoint, restored, parseError)) {
        error = "checkpoint payload is invalid: " + parseError.message;
        return false;
    }
    return WriteFileExclusive(path, checkpoint, economy::MaxCheckpointBytes, error);
}

} // namespace openrepublic::runtime
