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

add_executable(
  NestTryCatchTest
  NestTryCatchTest.cpp
)

target_link_libraries(
  NestTryCatchTest
  gtest_main
  ${llvm_libs}
  egp
  ica
  ics
  util
)

add_executable(
  PureVirtualTest
  PureVirtualTest.cpp
)

target_link_libraries(
  PureVirtualTest
  gtest_main
  ${llvm_libs}
  ica
)
