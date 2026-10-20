#pragma once

#include <proxy.hpp>
#include <proxy/reflection.hpp>

namespace proxy_test_abi {
struct[[= proxy::spec]]
      [[= proxy::uuid("proxy.tests.abi.root")]] IRoot {
    int root_value() const;
};

struct[[= proxy::spec]]
      [[= proxy::uuid("proxy.tests.abi.extra")]] IExtra {
    int extra_value() const;
};

struct[[= proxy::spec]]
      [[= proxy::uuid("proxy.tests.abi.absent")]] IAbsent {
    int absent_value() const;
};
}
