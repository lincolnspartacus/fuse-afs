#include <iostream>
#include <memory>
#include <fstream>

#include <grpcpp/grpcpp.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <sys/fcntl.h>


#include "dfs.grpc.pb.h"
#include "dfs.pb.h"

#include "cmake/build/_deps/grpc-src/include/grpcpp/channel.h"

//#define DEBUG
#ifdef DEBUG
#define print(x) std::cout << x;
#define println(x) std::cout << x << "\n";
#else
#define print(x)
#define println(x)
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using dfs::FileSystem;
using dfs::GenericRequest;
using dfs::GenericResponse;
using dfs::StatResponse;
using dfs::ReaddirResponse;
using dfs::Bytes;

enum class Method {
    MKDIR,
    RMDIR,
    RM,
    CREAT,
    STAT,
    READDIR
};

enum class FileStatus: int {
    ALREADY_CACHED = 1,
    FILE_ERROR = 2
};

class FilesystemClient {
public:
    FilesystemClient(std::shared_ptr<Channel> channel)
            : stub_(dfs::FileSystem::NewStub(channel)) {}

    std::pair<int, unsigned long long> open(std::string filepath, std::string cache_filepath, unsigned long long timestamp) {
        ClientContext context;
        Bytes bytes;
        GenericRequest genericRequest;
        genericRequest.set_path(filepath);
        genericRequest.set_changetime(timestamp);

        std::unique_ptr <ClientReader<Bytes>> reader(
                stub_->open(&context, genericRequest));
        std::ofstream fileStream;

        while (reader->Read(&bytes)) {
            if(bytes.status() == int(FileStatus::ALREADY_CACHED)) {
                println(filepath + " is ALREADY_CACHED");
//                std::cout << filepath << " is ALREADY_CACHED" << "\n";
                return {0, 0};
            } else if(bytes.status() == int(FileStatus::FILE_ERROR)) {
                println(filepath + " couldn't be opened on the server");
//                std::cout << filepath << " couldn't be opened on the server\n";
                return {-1, 0};
            }

            if(!fileStream.is_open())
                fileStream.open(cache_filepath, std::ios::binary | std::ofstream::trunc);
            fileStream << bytes.data();
        }
        fileStream.close();

        Status status = reader->Finish();

        if (status.ok()) {
            println("Read Successfully");
//            std::cout << "Read Successfully" << "\n";
            return {1, bytes.changetime()};
        } else {
            println(int(std::to_string(int(status.error_code()))) + ": " + status.error_message());
//            std::cout << status.error_code() << ": " << status.error_message()
//                      << std::endl;
            return {-1, 0};
        }
    }

    int close(std::string filepath, std::string cache_filepath, GenericResponse &resp) {
        int chunkSize = 65536;
        bool firstSend = true;
        ClientContext context;
        GenericResponse genericResponse;
        Bytes bytes;
        std::unique_ptr <ClientWriter<Bytes>> writer(
                stub_->close(&context, &genericResponse));
        char *buffer = new char[chunkSize];
        std::ifstream fileStream;
        fileStream.open(cache_filepath, std::ios::binary);

        bytes.set_path(filepath);

        while (!fileStream.eof()) {
            bytes.clear_data();
            fileStream.read(buffer,chunkSize);
            bytes.set_data(std::string(buffer, fileStream.gcount()));
            writer->Write(bytes);
        }
        delete[] buffer;
        fileStream.close();
        writer->WritesDone();
        Status status = writer->Finish();

        resp = genericResponse;

        if (status.ok()) {
            println("Server Time: " + std::to_string(genericResponse.changetime()) + " Server Response: "
                                    + std::to_string(genericResponse.responsecode()));
//            std::cout << "Server Time: " << genericResponse.changetime() << " Server Response: "
//                      << genericResponse.responsecode() << "\n";
            return genericResponse.responsecode();
        } else {
            println(std::to_string(int(status.error_code())) + ": " + status.error_message());
//            std::cout << status.error_code() << ": " << status.error_message()
//                      << std::endl;
            return -1;
        }

    }

    std::pair<bool, ReaddirResponse> readdir(const Method method, const std::string &path, const std::vector<std::string> &params,
                                                const std::vector<int> &flags) {
        Status status;
        ClientContext context;
        ReaddirResponse readdirResponse;
        GenericRequest genericRequest;
        genericRequest.set_path(path);

        status = stub_->readdir(&context, genericRequest, &readdirResponse);

        if (status.ok()) {
            println("Server Response: " + std::to_string(readdirResponse.responsecode()));
//            std::cout <<  " Server Response: "
//                      << readdirResponse.responsecode() << "\n";
            return {true, readdirResponse};
        } else {
            println(std::to_string(int(status.error_code())) + ": " + status.error_message());
//            std::cout << status.error_code() << ": " << status.error_message()
//                      << std::endl;
            return {false, readdirResponse};
        }
    }

    std::pair<bool, StatResponse> stat(const std::string &path) {
        Status status;
        ClientContext context;
        StatResponse statResponse;
        GenericRequest genericRequest;
        genericRequest.set_path(path);

        status = stub_->stat(&context, genericRequest, &statResponse);

        if (status.ok()) {
            println("Server Time: " + std::to_string(statResponse.changetime()) + " Server Response: "
                                    + std::to_string(statResponse.responsecode()));
//            std::cout << "Server Time: " << statResponse.changetime() << " Server Response: "
//                      << statResponse.responsecode() << "\n";
            return {true, statResponse};
        } else {
            println(std::to_string(int(status.error_code())) + ": " + status.error_message());
//            std::cout << status.error_code() << ": " << status.error_message()
//                      << std::endl;
            return {false, statResponse};
        }
    }

    std::pair<bool, GenericResponse> CallMethod(const Method method, const std::string &path, const std::vector<std::string> &params,
                   const std::vector<int> &flags) {
        Status status;
        ClientContext context;
        GenericResponse genericResponse;
        GenericRequest genericRequest;
        genericRequest.set_path(path);
        for (auto p: params)
            genericRequest.add_params(p);
        for (auto f: flags)
            genericRequest.add_flag(f);

        switch (method) {
            case Method::MKDIR:
                status = stub_->mkdir(&context, genericRequest, &genericResponse);
                break;
            case Method::RMDIR:
                status = stub_->rmdir(&context, genericRequest, &genericResponse);
                break;
            case Method::RM:
                status = stub_->rm(&context, genericRequest, &genericResponse);
                break;
            case Method::CREAT:
                status = stub_->creat(&context, genericRequest, &genericResponse);
                break;
            default:
                break;
        }

        if (status.ok()) {
            println("Server Time: " + std::to_string(genericResponse.changetime()) + " Server Response: "
                    + std::to_string(genericResponse.responsecode()));
           // std::cout << "Server Time: " << genericResponse.changetime() << " Server Response: "
           //           << genericResponse.responsecode() << "\n";
            return {true, genericResponse};
        } else {
            println(std::to_string(int(status.error_code())) + ": " + status.error_message());
//            std::cout << status.error_code() << ": " << status.error_message()
//                      << std::endl;
            return {false, genericResponse};
        }
    }

private:
    std::unique_ptr<FileSystem::Stub> stub_;
};

/*int main(int argc, char **argv) {
    std::string target_str = "localhost:50051";
    FilesystemClient client(
            grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
    std::vector<std::string> v = {"-p"};
    std::vector<int> f = {S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH};
//    int response = client.CallMethod(Method::MKDIR, "/home/kaustubh", v);
    client.CallMethod(Method::MKDIR, "Users", v, f);
    client.CallMethod(Method::MKDIR, "Users/kaustubh", v, f);
    client.CallMethod(Method::MKDIR, "Users/kaustubh/Development", v, f);
    client.CallMethod(Method::MKDIR, "Users/kaustubh/Development/CLionProjects", v, f);
    client.CallMethod(Method::MKDIR, "Users/kaustubh/Development/CLionProjects/DistributedFileSystem", v, f);
    client.close("/Users/kaustubh/Development/CLionProjects/DistributedFileSystem/timing.h");
//    client.open("/Users/kaustubh/Development/CLionProjects/DistributedFileSystem/tmp/build.sh");

    std::pair<bool, ReaddirResponse> pair = client.readdir(Method::READDIR, "/Users/kaustubh/Development/CLionProjects/DistributedFileSystem/tmp/", v, f);
    ReaddirResponse resp = pair.second;

    for (auto f: resp.files()) {
        std::cout << "file: " << f << "\n";
    }

    f.clear();
    f.push_back(O_CREAT);
    client.CallMethod(Method::CREAT, "a.cpp", v, f);

    return 0;
}*/
