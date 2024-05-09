add_executable(ehgp
  main.cpp
  IndirectCallSolver.cpp
  IndirectCallAnalysis.cpp
  EHGraphPrinter.cpp
  util.cpp
)

target_link_libraries(ehgp ${llvm_libs})

add_library(ica IndirectCallAnalysis.cpp)

target_link_libraries(ica ${llvm_libs})

add_library(ics IndirectCallSolver.cpp)
target_link_libraries(ics ${llvm_libs})

add_library(egp EHGraphPrinter.cpp)
target_link_libraries(egp ${llvm_libs})

add_library(util util.cpp)
target_link_libraries(util ${llvm_libs})
