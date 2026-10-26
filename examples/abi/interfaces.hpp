#pragma once

#include <facet.hpp>
#include <facet/reflection.hpp>

namespace facet_test_abi {
struct[[= fct::spec]]
      [[= fct::id("facet.tests.abi.root")]] IRoot {
    int root_value() const;
};

struct[[= fct::spec]]
      [[= fct::id("facet.tests.abi.extra")]] IExtra {
    int extra_value() const;
};

struct[[= fct::spec]]
      [[= fct::id("facet.tests.abi.absent")]] IAbsent {
    int absent_value() const;
};
}
