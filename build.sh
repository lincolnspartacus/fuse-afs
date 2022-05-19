HOME="$(pwd)/"
mkdir -p ${HOME}cmake/build
PROTOC=${HOME}cmake/build/_deps/grpc-build/third_party/protobuf/protoc
PROTO_PATH=${HOME}protos
OUT_PATH=${HOME}
PROTOC_GRPC=${HOME}cmake/build/_deps/grpc-build/grpc_cpp_plugin

if [[ ! -f "cmake/build/Makefile" ]] || [[ ! -d "cmake/build/_deps" ]]
then
	echo "Running cmake"
	cp CMakeLists.txt.tmp CMakeLists.txt
	cd ${HOME}/cmake/build
	cmake ../../
	make -j8
	cd ${HOME}
	cp CMakeLists.txt.orig CMakeLists.txt
fi

rm ${OUT_PATH}dfs.*
echo "Executing protoc: $PROTOC --proto_path=$PROTO_PATH --cpp_out=$OUT_PATH --grpc_out=$OUT_PATH --plugin=protoc-gen-grpc=$PROTOC_GRPC dfs.proto"
$PROTOC --proto_path=$PROTO_PATH --cpp_out=$OUT_PATH --grpc_out=$OUT_PATH --plugin=protoc-gen-grpc=$PROTOC_GRPC dfs.proto

echo "Executing make"
cd ${HOME}cmake/build
make -j8

