add_executable(
  SubClassTest
  SubClassTest.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
  SubClassTest
  gtest_main
  ${llvm_libs}
  ica
)
