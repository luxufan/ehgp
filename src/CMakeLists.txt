add_executable(ehgp
  main.cpp
  IndirectCallAnalysis.cpp
  EHGraphPrinter.cpp
  util.cpp
)

target_link_libraries(ehgp ${llvm_libs})

add_library(ica IndirectCallAnalysis.cpp)

target_link_libraries(ica ${llvm_libs})
