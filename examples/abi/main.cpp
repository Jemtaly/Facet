#include <cassert>

#include "interfaces.hpp"

#if defined(_WIN32)
#define FACET_TEST_IMPORT __declspec(dllimport)
#else
#define FACET_TEST_IMPORT
#endif

extern "C" FACET_TEST_IMPORT void facet_test_make_root(
    fct::facets<facet_test_abi::IRoot> *out);

int main() {
    fct::facets<facet_test_abi::IRoot> root;
    facet_test_make_root(&root);
    assert(root);
    assert(root->root_value() == 17);

    fct::facets<facet_test_abi::IExtra> extra{root};
    assert(extra);
    assert(extra->extra_value() == 29);

    fct::facets<facet_test_abi::IAbsent> absent{root};
    assert(!absent);
}
