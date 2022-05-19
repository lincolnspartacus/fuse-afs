cmake_minimum_required(VERSION 3.15)
project(Filesystem)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
        gRPC
        GIT_REPOSITORY https://github.com/grpc/grpc
        GIT_TAG        v1.28.0
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(gRPC)

#add_executable(Filesystem main.cc dfs.grpc.pb.cc dfs.pb.cc)
add_executable(Client Cache.cc RPC.cc client_fuse.cc dfs.grpc.pb.cc dfs.pb.cc)
add_executable(Server server.cc dfs.grpc.pb.cc dfs.pb.cc)
#target_link_libraries(Filesystem grpc++)
target_link_libraries(Client grpc++ stdc++fs fuse pthread)
target_link_libraries(Server grpc++ stdc++fs)
