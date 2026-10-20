#include "interfaces.hpp"

#if defined(_WIN32)
#define FACET_TEST_EXPORT __declspec(dllexport)
#else
#define FACET_TEST_EXPORT __attribute__((visibility("default")))
#endif

namespace {
struct PluginService {
    int root_value() const {
        return 17;
    }

    int extra_value() const {
        return 29;
    }
};

PluginService service;
}

extern "C" FACET_TEST_EXPORT void facet_test_make_root(
    fct::interface_handle<facet_test_abi::IRoot> *out) {
    auto handle = fct::as_handle<facet_test_abi::IRoot, facet_test_abi::IExtra>(&service);
    *out = fct::interface_handle<facet_test_abi::IRoot>(handle);
}
