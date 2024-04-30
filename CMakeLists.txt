project(ehprinter)

set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "-fno-rtti")

set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

add_executable(ehprinter main.cpp IndirectCallAnalysis.cpp EHInfer.cpp)

llvm_map_components_to_libnames(llvm_libs support core irreader codegen passes)

target_link_libraries(ehprinter ${llvm_libs})
