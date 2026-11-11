#include <cstdint>
#ifdef _WIN32
#define QPX_TEST_EXPORT __declspec(dllexport)
#else
#define QPX_TEST_EXPORT
#endif
extern "C" QPX_TEST_EXPORT std::uint64_t qpx_reader_test_dependency() {return UINT64_C(9007199254740995);}
