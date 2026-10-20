#pragma once

#include <facet.hpp>
#include <facet/reflection.hpp>

#ifndef FACET_TEST_API_LEVEL
#define FACET_TEST_API_LEVEL 2
#endif

namespace facet_test_evolution {
struct[[= fct::spec]]
      [[= fct::uuid("facet.tests.versioned-api")]]
      [[= fct::version(FACET_TEST_API_LEVEL)]] IRevisioned {
    int value() const;

#if FACET_TEST_API_LEVEL >= 2
    [[= fct::since(2, [](auto self, int delta) {
        return self->value() + delta;
    })]] int value_after_upgrade(int delta) const;
#endif
};
}
