#define PROXY_TEST_API_LEVEL 1
#include "versioned_api.hpp"

#if defined(_WIN32)
#define PROXY_TEST_EXPORT __declspec(dllexport)
#else
#define PROXY_TEST_EXPORT __attribute__((visibility("default")))
#endif

namespace {
struct VersionOneService {
    int value() const {
        return 10;
    }
};

VersionOneService service;
}

extern "C" PROXY_TEST_EXPORT void proxy_test_make_v1(
    proxy::interface_handle<proxy_test_evolution::IRevisioned> *out) {
    *out = proxy::as_handle<proxy_test_evolution::IRevisioned>(&service);
}
