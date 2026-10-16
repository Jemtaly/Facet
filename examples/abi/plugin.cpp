#include "interfaces.hpp"

#if defined(_WIN32)
#define PROXY_TEST_EXPORT __declspec(dllexport)
#else
#define PROXY_TEST_EXPORT __attribute__((visibility("default")))
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

extern "C" PROXY_TEST_EXPORT void proxy_test_make_root(
    proxy::interface_handle<proxy_test_abi::IRoot> *out) {
    auto handle = proxy::as_handle<proxy_test_abi::IRoot, proxy_test_abi::IExtra>(&service);
    *out = proxy::interface_handle<proxy_test_abi::IRoot>(handle);
}
