include(FetchContent)
# Only the standalone Math include tree participates in compilation. Do not add
# upstream's test/doc/CMake project or fetch the full Boost dependency graph.
FetchContent_Declare(qpx_boost_math
  URL https://codeload.github.com/boostorg/math/tar.gz/refs/tags/boost-1.88.0
  URL_HASH SHA256=bb753392c3908292da46034a5435107f9e0546fda54da4d47b438aff5e4d9c48
  DOWNLOAD_NAME boost-math-1.88.0.tar.gz
  DOWNLOAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../dependency-downloads"
  SOURCE_SUBDIR qupathxx-headers-only)
FetchContent_MakeAvailable(qpx_boost_math)
set_source_files_properties(cpp/expression_differential.cpp PROPERTIES
  COMPILE_DEFINITIONS "QUPATHXX_HAS_STATISTICS;BOOST_MATH_STANDALONE"
  INCLUDE_DIRECTORIES "${qpx_boost_math_SOURCE_DIR}/include")
install(FILES third_party/boost-math-LICENSE_1_0.txt DESTINATION qupathxx/licenses/boost-math RENAME LICENSE_1_0.txt)
