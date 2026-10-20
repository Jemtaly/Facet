#include <cassert>

#define FACET_TEST_API_LEVEL 2
#include "versioned_api.hpp"

#if defined(_WIN32)
#define FACET_TEST_IMPORT __declspec(dllimport)
#else
#define FACET_TEST_IMPORT
#endif

extern "C" FACET_TEST_IMPORT void facet_test_make_v1(
    fct::interface_handle<facet_test_evolution::IRevisioned> *out);

int main() {
    fct::interface_handle<facet_test_evolution::IRevisioned> old_handle;
    facet_test_make_v1(&old_handle);
    assert(old_handle);
    assert(old_handle->value() == 10);
    assert(old_handle->value_after_upgrade(5) == 15);
}
