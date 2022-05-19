#include <unordered_map>
#include <experimental/filesystem>
#include <cassert>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "Cache.h"


static std::string CACHE_DIR;
static const std::string CACHE_FILE = "/.cache_last_modified";

// In-memory cache map : <path, last_modified_time> pairs
static std::unordered_map<std::string, unsigned long long> cmap;

/*
 * Initializes in-memory cache mapping from the cache mapping file.
 * Returns : void
 */
void Cache::init(const std::string user_cache_dir)
{
    CACHE_DIR = user_cache_dir;

    std::string cache_full_path = CACHE_DIR + CACHE_FILE;
    FILE *cache_file = fopen(cache_full_path.c_str(), "r");

    if(cache_file) {
        char *buf = (char*) malloc(4096);
        size_t buf_size = 4096;
        while(getline(&buf, &buf_size, cache_file) != -1) {
            if(buf[0] == '\n') // ignore empty lines
                continue;

            std::string tmp{buf};
            int comma_index = tmp.find(",");
            assert(comma_index > 0);
            std::string filename = tmp.substr(0, comma_index);
            unsigned long long modified_time;
            sscanf(buf + comma_index + 1, "%llu", &modified_time);            
            printf("[%s] cache_last_modified : %s, %llu\n", __func__, filename.c_str(), modified_time);

            cmap[filename] = modified_time;
        }
        free(buf);
        fclose(cache_file);
    }

    
}

/*
 * Checks if file is present in cache.
 * Returns : last modified time of file (if found), else 0
 * param : path to file
 */
unsigned long long Cache::lookup(const char *path)
{
    std::unordered_map<std::string, unsigned long long>::const_iterator it;
    it = cmap.find(path);

    if(it == cmap.end())
        return 0;

    return it->second;
}

/*
 * Flushes the unordered_map cache to disk with a local update protocol
 */
static void cache_flush_disk()
{
    std::string cache_full_path_tmp = CACHE_DIR + CACHE_FILE + "2";
    int fd = open(cache_full_path_tmp.c_str(), O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU|S_IRGRP|S_IROTH);

    assert(fd >= 0);

    for(auto i : cmap) {
        write(fd, i.first.c_str(), i.first.length());
        write(fd, ",", 1);
        std::string timestamp_str = std::to_string(i.second);
        write(fd, timestamp_str.c_str(), timestamp_str.length());
        write(fd, "\n", 1);
    }

    fsync(fd);
    close(fd);

    // Perform atomic rename of .cache_last_modified2 -> .cache_last_modified
    std::string cache_full_path = CACHE_DIR + CACHE_FILE;
    int rc = rename(cache_full_path_tmp.c_str(), cache_full_path.c_str());
    assert(rc >= 0);
}


/*
 * Updates contents of in-memory cache and flushes it to disk
 * with a local update protocol
 * Returns : void
 * params : key-value pair <filename, timestamp> to be inserted
 */
void Cache::update_entry(const char *path, unsigned long long timestamp)
{
    cmap[path] = timestamp;
    cache_flush_disk();
}

/*
 * Deletes an entry from the in-memory cache and flushes it to disk
 * Returns : void
 * params : filename to be deleted
 */
void Cache::delete_entry(const char *path)
{
    cmap.erase(path);
    cache_flush_disk();
}

/*
 * Simulates 'mkdir -p' for the specified folder path
 * inside our cache directory
 * Returns : void
 * param : path to folder (eg : /a/b/c)
 */
void Cache::mkdir(const char *path)
{
    std::string full_path = CACHE_DIR + path;
    int rc = std::experimental::filesystem::create_directories(full_path);
    //assert(rc > 0);
}

/*
 * Creates an empty file with the specified path
 * inside our cache directory. Also creates all
 * necessary directories along the way.
 * Returns : void
 * param : path to file (eg : /a/b/c/file.txt)
 */
int Cache::create_file(const char *path,uint64_t flags, mode_t mode)
{
    std::experimental::filesystem::path root_path(path);
    // Create the directory structure for our file
    if(root_path.parent_path() != "/")
        Cache::mkdir(root_path.parent_path().c_str());
    
    std::string full_path = CACHE_DIR + path;
    //std::string full_path = SERVER_PATH + path;
    int fd = open(full_path.c_str(), flags | O_CREAT | O_TRUNC, mode);
    fsync(fd);
    return fd;

}
