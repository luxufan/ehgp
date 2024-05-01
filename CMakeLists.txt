cmake_minimum_required(VERSION 3.29)
project(ehprinter)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_CXX_FLAGS "-fno-rtti")

set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

add_executable(ehprinter main.cpp IndirectCallAnalysis.cpp EHGraphPrinter.cpp)

llvm_map_components_to_libnames(llvm_libs support core irreader codegen passes)

target_link_libraries(ehprinter ${llvm_libs})

add_subdirectory(third-party/googletest)
add_subdirectory(unittest)

add_library(ica IndirectCallAnalysis.cpp)
target_link_libraries(ica ${llvm_libs})
