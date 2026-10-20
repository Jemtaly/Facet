#define FACET_TEST_API_LEVEL 1
#include "versioned_api.hpp"

#if defined(_WIN32)
#define FACET_TEST_EXPORT __declspec(dllexport)
#else
#define FACET_TEST_EXPORT __attribute__((visibility("default")))
#endif

namespace {
struct VersionOneService {
    int value() const {
        return 10;
    }
};

VersionOneService service;
}

extern "C" FACET_TEST_EXPORT void facet_test_make_v1(
    fct::interface_handle<facet_test_evolution::IRevisioned> *out) {
    *out = fct::as_handle<facet_test_evolution::IRevisioned>(&service);
}
