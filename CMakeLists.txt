cmake_minimum_required(VERSION 3.29)
project(ehprinter)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_CXX_FLAGS "-fno-rtti")

set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(include)
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader codegen passes)

add_subdirectory(src)
add_subdirectory(third-party/googletest)
add_subdirectory(unittest)

