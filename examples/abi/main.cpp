#include <cassert>

#include "interfaces.hpp"

#if defined(_WIN32)
#define PROXY_TEST_IMPORT __declspec(dllimport)
#else
#define PROXY_TEST_IMPORT
#endif

extern "C" PROXY_TEST_IMPORT void proxy_test_make_root(
    proxy::interface_handle<proxy_test_abi::IRoot> *out);

int main() {
    proxy::interface_handle<proxy_test_abi::IRoot> root;
    proxy_test_make_root(&root);
    assert(root);
    assert(root->root_value() == 17);

    proxy::interface_handle<proxy_test_abi::IExtra> extra{root};
    assert(extra);
    assert(extra->extra_value() == 29);

    proxy::interface_handle<proxy_test_abi::IAbsent> absent{root};
    assert(!absent);
}
