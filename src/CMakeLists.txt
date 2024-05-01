add_executable(ehp
  main.cpp
  IndirectCallAnalysis.cpp
  EHGraphPrinter.cpp
  util.cpp
)

target_link_libraries(ehp ${llvm_libs})

add_library(ica IndirectCallAnalysis.cpp)

target_link_libraries(ica ${llvm_libs})
