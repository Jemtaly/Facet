#pragma once

#include <proxy.hpp>
#include <proxy/reflection.hpp>

#ifndef PROXY_TEST_API_LEVEL
#define PROXY_TEST_API_LEVEL 2
#endif

namespace proxy_test_evolution {
struct[[= proxy::spec]]
      [[= proxy::uuid("proxy.tests.versioned-api")]]
      [[= proxy::version(PROXY_TEST_API_LEVEL)]] IRevisioned {
    int value() const;

#if PROXY_TEST_API_LEVEL >= 2
    [[= proxy::since(2, [](auto self, int delta) {
        return self->value() + delta;
    })]] int value_after_upgrade(int delta) const;
#endif
};
}
