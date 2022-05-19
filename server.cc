#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <experimental/filesystem>
#include <iomanip>
#include <functional>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "dfs.grpc.pb.h"
#include "dfs.pb.h"
#include "timing.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using dfs::FileSystem;
using dfs::GenericResponse;
using dfs::GenericRequest;
using dfs::ReaddirResponse;
using dfs::StatResponse;
using dfs::Bytes;

unsigned int fileCount = 0;
pthread_mutex_t count_mutex;

unsigned int increment_count()
{
    unsigned int ret;
    pthread_mutex_lock(&count_mutex);
    ret = fileCount++;
    pthread_mutex_unlock(&count_mutex);

    return ret;
}

enum class ResponseCode: int {
    OK = 1,
    ERROR = -1
};

enum class FileStatus: int {
    ALREADY_CACHED = 1,
    FILE_ERROR = 2
};

/*struct hash_input {
    std::string random_no;
    std::string time_n;
};*/

/*template<>
struct std::hash<hash_input>
{
    unsigned int operator()(hash_input const& s) const noexcept
    {

        unsigned int h1 = std::hash<std::string>{}(s.time_n);
        unsigned int h2 = std::hash<std::string>{}(s.random_no);
        return h1 ^ (h2 << 1); // or use boost::hash_combine
    }
};*/

std::string BASE_PATH;

class FilesystemServer final : public FileSystem::Service {

    struct stat getStats(std::string filepath) {
        struct stat s;
        ::stat(filepath.c_str(), &s);
        return s;
    }

    unsigned long int getChangeTime(std::string filepath) {
        return getStats(filepath).st_mtime;
//        return getStats(filepath).st_mtim.tv_nsec; TODO: for x86
    }

    Status creat(ServerContext* context, const GenericRequest* request,
                 GenericResponse* response) override {
        int fd = ::creat((BASE_PATH + request->path()).c_str(), request->flag(0));
        if(fd < 0)
            response->set_errorcode(errno);
        else
            response->set_errorcode(0); // This means everything is good. errno is never set to 0 by any syscall
        response->set_responsecode(int(ResponseCode::OK));
        response->set_changetime(getChangeTime(request->path()));
//        std::cout<<"creat"<<std::endl;
        return Status::OK;
    }

    Status mkdir(ServerContext* context, const GenericRequest* request,
                 GenericResponse* response) override {
        int fd=::mkdir((BASE_PATH + request->path()).c_str(), request->flag(0));
        if(fd < 0)
            response->set_errorcode(errno);
        else
            response->set_errorcode(0); // This means everything is good. errno is never set to 0 by any syscall
        response->set_responsecode(int(ResponseCode::OK));
        response->set_changetime(getChangeTime(request->path()));
//        std::cout << "mkdir" << "\n";
        return Status::OK;
    }

    Status rm(ServerContext* context, const GenericRequest* request,
                 GenericResponse* response) override {
        int res=::unlink((BASE_PATH + request->path()).c_str());
        if(res < 0)
            response->set_errorcode(errno);
        else
            response->set_errorcode(0); // This means everything is good. errno is never set to 0 by any syscall
        response->set_responsecode(int(ResponseCode::OK));
        response->set_changetime(time_monotonic());
//        std::cout << "rm" << "\n";
        return Status::OK;
    }

    Status rmdir(ServerContext* context, const GenericRequest* request,
                 GenericResponse* response) override {
        int res=::remove((BASE_PATH + request->path()).c_str());
        if(res < 0)
            response->set_errorcode(errno);
        else
            response->set_errorcode(0); // This means everything is good. errno is never set to 0 by any syscall
        response->set_responsecode(int(ResponseCode::OK));
        response->set_changetime(time_monotonic());
//        std::cout << "rmdir" << "\n";
        return Status::OK;
    }

    /**
     * Taken from https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
     */
    std::string* getFilename(std::string& path, std::string delimiter) {
        std::string *s = new std::string(path);
        size_t pos = 0;
        std::string token;
        while ((pos = s->find(delimiter)) != std::string::npos) {
            token = s->substr(0, pos);
            s->erase(0, pos + delimiter.length());
        }

        return s;
    }

    Status readdir(ServerContext* context, const GenericRequest* request,
                 ReaddirResponse* response) override {
        for (const auto &path : std::experimental::filesystem::directory_iterator(BASE_PATH + request->path())) {
            std::string* file = getFilename((std::string &) path, "/");
            response->add_files(*file);
        }
        response->set_responsecode(int(ResponseCode::OK));
//        std::cout << "readdir" << "\n";
        return Status::OK;
    }

    Status stat(ServerContext* context, const GenericRequest* request,
                 StatResponse* response) override {
        //struct stat s = getStats((BASE_PATH + request->path()));
        std::string filepath = BASE_PATH + request->path();
        struct stat s;
        int rc=lstat(filepath.c_str(), &s);
        response->set_size(s.st_size);
        response->set_responsecode(int(ResponseCode::OK));
        response->set_changetime(s.st_mtime);
        response->set_nlink(s.st_nlink);
        response->set_mode(s.st_mode);
        response->set_accesstime(s.st_atime);
        response->set_returncode(rc);
        response->set_errorno(errno);
//        std::cout << "stat : rc = "<<rc << "\n";
        return Status::OK;
    }

    Status close(ServerContext* context, ServerReader<Bytes>* reader,
                        GenericResponse* genericResponse) override {



        Bytes bytes;
        std::ofstream fileStream;
        /*int r=rand();
        struct timespec time;
        clock_gettime(CLOCK_REALTIME, &time);
        hash_input obj = { std::to_string(r), std::to_string(time.tv_sec+time.tv_nsec) };
        unsigned int tmp_hash=std::hash<hash_input>{}(obj);*/
        //std::cout<<"Hash value "<<tmp_hash<<std::endl;
        reader->Read(&bytes);

        // Make recursive directory structure
        std::experimental::filesystem::path filepath(bytes.path());
        std::string parent_dir_path = BASE_PATH + filepath.parent_path().string();
        std::experimental::filesystem::create_directories(parent_dir_path);

        std::string tmp_file_path = BASE_PATH + "/tmp" + std::to_string(increment_count()); //(BASE_PATH + bytes.path() + std::to_string(tmp_hash));
//        std::cout<<"tmp_file path "<<tmp_file_path<<std::endl;
        fileStream.open(tmp_file_path, std::ios::binary);

        fileStream << bytes.data();
        while (reader->Read(&bytes)) {
            fileStream << bytes.data();
        }
        fileStream.flush();
        fileStream.close();
        ::rename(tmp_file_path.c_str(),(BASE_PATH + bytes.path()).c_str());
        genericResponse->set_responsecode(int(ResponseCode::OK));
        genericResponse->set_changetime(getChangeTime((BASE_PATH + bytes.path())));

        return Status::OK;
    }

    Status open(ServerContext *context,
                      const dfs::GenericRequest *genericRequest,
                      ServerWriter <Bytes> *writer) override {
        int chunkSize = 65536;
        char *buffer = new char[chunkSize];
        std::string filepath = BASE_PATH + genericRequest->path();
        Bytes bytes;

//        std::cout << getChangeTime(filepath) << "\n";;
//        std::cout << genericRequest->changetime() << "\n";
        if (getChangeTime(filepath) == genericRequest->changetime()) {
//            std::cout << "Server : Inside changeTime matched" << "\n";
            bytes.set_status(int(FileStatus::ALREADY_CACHED));
            bytes.clear_data();
            bytes.clear_path();
            writer->Write(bytes);
            return Status::OK;
        }

        std::ifstream fileStream;
        fileStream.open(filepath, std::ios::binary);
        bytes.set_changetime(getChangeTime(filepath));

        if (!fileStream.is_open() || fileStream.bad()) {
            bytes.set_status(int(FileStatus::FILE_ERROR));
            bytes.clear_data();
            bytes.clear_path();
            writer->Write(bytes);
            return Status::OK;
        }

        while (!fileStream.eof()) {
            bytes.clear_data();
            fileStream.read(buffer,chunkSize);
            bytes.set_path("");
            bytes.set_data(std::string(buffer, fileStream.gcount()));
            writer->Write(bytes);
        }
        delete[] buffer;
        fileStream.close();

        return Status::OK;
    }
};

int main(int argc, char** argv) {

    if(argc > 3) {
        printf("Usage : ./Server -dir <dir-name>\n");
        return 0;
    }

    BASE_PATH = "server"; // Default path, if nothing specified
    for(int i = 1; i < argc - 1; ++i) {
        if(!strcmp(argv[i], "-dir")) {
            BASE_PATH = std::string{argv[i+1]};
        }
    }

    assert(pthread_mutex_init(&count_mutex, NULL) == 0);

    std::string server_address("0.0.0.0:50051");
    FilesystemServer service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << " with server directory " << BASE_PATH << "\n";

    server->Wait();

    return 0;
}
