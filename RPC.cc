#include <string>
#include <vector>
#include <experimental/filesystem>
#include <sys/stat.h>
#include <utime.h>
#include "RPC.h"
#include "Cache.h"
#include "client.h"

static std::string CACHE_DIR;

#define debug 0

FilesystemClient *client;
void RPC::init(const std::string ip, const std::string user_cache_dir){
    //std::string target_str = "localhost:50051";
    CACHE_DIR = user_cache_dir;
    std::cout << "Creating channel with " << ip << " with cache directory " << user_cache_dir << "\n";
    client= new FilesystemClient(
    grpc::CreateChannel(ip, grpc::InsecureChannelCredentials()));
}
int RPC::client_getattr(const char *path, struct stat *st)
{
    int rc;
    std::pair<bool, StatResponse> resp;
    resp=client->stat(path);
    
    if(resp.first == false)
        return -EIO;

    rc=resp.second.returncode();
    if(rc==-1)
        return -resp.second.errorno();
    st->st_mtime = resp.second.changetime();
    st->st_mode =  resp.second.mode();
    st->st_nlink = resp.second.nlink();
    st->st_size = resp.second.size();
    st->st_atime = resp.second.accesstime();

    return rc;
}

int RPC::client_readdir(const char *path, std::vector<std::string> &dir_entries)
{
    std::vector<std::string> v;
    std::vector<int> f;
    std::pair<bool, ReaddirResponse> resp;
    resp = client->readdir(Method::READDIR, path, v, f);

    if(resp.first == false)
        return -1;

    for(auto file_name : resp.second.files()) {
        dir_entries.push_back(file_name);
    }

    return 0;
}

int RPC::client_open(const char *path,uint64_t flags){

    int fd;

    std::experimental::filesystem::path filepath(path);
    auto fs_tmp_path = filepath.parent_path() / (filepath.filename().string() +  "._zxassf1353467");
    std::string cache_tmp_filepath = CACHE_DIR + fs_tmp_path.string();
    std::string cache_final_filepath = CACHE_DIR + path;

    // Create directory structure inside cache dir
    Cache::mkdir(filepath.parent_path().c_str());

    auto resp = client->open(path, cache_tmp_filepath, Cache::lookup(path));
    int rc = resp.first;

    if (debug)
    printf("RPC::client_open : rc = %d\n", rc);
    // rc = 0 -> file is already cached
    // rc = -1 -> gRPC error
    // rc = 1 -> Fetched new file
    if(rc < 0) {
        return -EIO;
    } else if (rc == 1) {
        // Atomic rename
        int rc_rename = rename(cache_tmp_filepath.c_str(), cache_final_filepath.c_str());
        if(rc_rename < 0)
            return -errno;
        
        //Update the modified timestamp to match with the server
        struct utimbuf times;
	    times.modtime = resp.second;
	    utime(cache_final_filepath.c_str(), &times);
    } else if (rc == 0) {
        // If file is already in cache, let user continue working on dirty data
    }

    // Return fd for the tmp copy
    fd = open(cache_final_filepath.c_str(), flags);
    if (debug)
    printf("RPC::open fd = %d\n", fd);

    if (rc == 1) {
        // Add timestamp to cache if newly fetched
        if (debug)
        printf("RPC::open flushing cache mapping file\n");
        if (debug)
        printf("timestamp = %lld\n", resp.second);
        Cache::update_entry(path, resp.second);
    }

    return fd;
}

int RPC::client_create(const char *path, uint64_t flags, mode_t mode){
    
    //Check if the file already exists
    std::pair<bool, GenericResponse> resp;
    std::vector<std::string> v = {"-p"};
    //std::vector<int> f = {O_CREAT| O_RDWR};
    std::vector<int> f = {O_CREAT | O_RDWR | S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH};
    resp=client->CallMethod(Method::CREAT, path, v, f);
    if(resp.first==false){
        return -EIO;
    }

    // Error creating file on server side
    if(resp.second.errorcode() != 0) {
        return -resp.second.errorcode();
    }
    
    int fd=Cache::create_file(path, flags,mode);
    if (fd == -1)
		return -errno;
        
    Cache::update_entry(path, resp.second.changetime());

    return fd;
}

int RPC::client_unlink(const char *path){

    std::pair<bool, GenericResponse> resp;
    std::vector<std::string> v = {"-p"};
    std::vector<int> f = {S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH};
    resp=client->CallMethod(Method::RM, path, v, f);
    if(resp.first==false){
        return -EIO;
    }
    
    Cache::delete_entry(path);

    std::string cache_final_filepath = CACHE_DIR + path;
	unlink(cache_final_filepath.c_str());

    //Even in case of error from server, do the unlink on files on cache silently?
    if(resp.second.errorcode() != 0) {
        return -resp.second.errorcode();
    }

    return 0;
}

int RPC::client_mkdir(const char *path, mode_t mode){
    std::pair<bool, GenericResponse> resp;
    std::vector<std::string> v = {"-p"};
    std::vector<int> f = {S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH};
    resp=client->CallMethod(Method::MKDIR, path, v, f);
    if(resp.first==false){
        return -EIO;
    }
    // Error on mkdir - server side
    if(resp.second.errorcode() != 0) {
        return -resp.second.errorcode();
    }

    return 0;
}

int RPC::client_rmdir(const char *path){
    std::pair<bool, GenericResponse> resp;
    std::vector<std::string> v = {"-p"};
    std::vector<int> f = {S_IRWXU};
    resp=client->CallMethod(Method::RMDIR, path, v, f);
    if(resp.first==false){
        return -EIO;
    }
    // Error on rmdir - server side
    if(resp.second.errorcode() != 0) {
        return -resp.second.errorcode();
    }

    return 0;
}

int RPC::client_close(const char *path){

    std::string cache_final_filepath = CACHE_DIR + path;
    GenericResponse resp;
    int rc = client->close(path, cache_final_filepath, resp);
    if(rc == 1) {
        // All ok
        // unset permissions
        //chmod(cache_final_filepath.c_str(), S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        //Update the modified timestamp to match with the server
        struct utimbuf times;
	    times.modtime = resp.changetime();
	    utime(cache_final_filepath.c_str(), &times);

        // update cache mapping with new last modified
        Cache::update_entry(path, resp.changetime());
        return 0;
    } else {
        // gRPC crapped itself
        return -EIO;
    }
}
