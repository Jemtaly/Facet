#include <cassert>

#define PROXY_TEST_API_LEVEL 2
#include "versioned_api.hpp"

#if defined(_WIN32)
#define PROXY_TEST_IMPORT __declspec(dllimport)
#else
#define PROXY_TEST_IMPORT
#endif

extern "C" PROXY_TEST_IMPORT void proxy_test_make_v1(
    proxy::interface_handle<proxy_test_evolution::IRevisioned> *out);

int main() {
    proxy::interface_handle<proxy_test_evolution::IRevisioned> old_handle;
    proxy_test_make_v1(&old_handle);
    assert(old_handle);
    assert(old_handle->value() == 10);
    assert(old_handle->value_after_upgrade(5) == 15);
}
