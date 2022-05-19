#include <string>
#include <vector>
#include <sys/stat.h>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>

namespace RPC {
    void init(const std::string ip, const std::string user_cache_dir);
    int client_readdir(const char *path, std::vector<std::string> &dir_entries);
    int client_getattr(const char *path, struct stat *st);
    int client_open(const char *path, uint64_t flags);
    int client_create(const char *path, uint64_t flags,mode_t mode);
    int client_unlink(const char *path);
    int client_mkdir(const char *path, mode_t mode);
    int client_rmdir(const char *path);
    int client_close(const char *path);
}
