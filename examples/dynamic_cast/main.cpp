#include <cassert>
#include <type_traits>

#include <proxy.hpp>
#include <proxy/reflection.hpp>

struct IRoot {
    int root() const;
};

struct IExtra {
    int extra() const;
};

struct IAbsent {
    int absent() const;
};

struct Model {
    int root() const {
        return 10;
    }

    int extra() const {
        return 20;
    }
};

void test_dynamic_cast() {
    Model model;
    auto initial = proxy::as_handle<IRoot, IExtra>(&model);
    proxy::interface_handle<IRoot> root = initial;

    static_assert(!std::is_convertible_v<
                  proxy::interface_handle<IExtra>,
                  proxy::interface_handle<IRoot>>);
    proxy::interface_handle<IExtra> extra{root};
    assert(extra);
    assert(extra->extra() == 20);

    static_assert(!std::is_convertible_v<
                  proxy::interface_handle<IAbsent>,
                  proxy::interface_handle<IRoot>>);
    proxy::interface_handle<IAbsent> absent{root};
    assert(!absent);
}

template<typename T>
struct ITemplate {
    T value() const;
};

struct TemplateModel {
    int value() const {
        return 42;
    }
};

void test_template_dynamic_cast() {
    TemplateModel model;
    auto handle = proxy::as_handle<ITemplate<int>>(&model);
    proxy::data_handle root = handle;

    proxy::interface_handle<ITemplate<int>> template_handle{root};
    assert(template_handle);
    assert(template_handle->value() == 42);

    proxy::interface_handle<ITemplate<double>> wrong_template_handle{root};
    assert(!wrong_template_handle);
}

int main() {
    test_dynamic_cast();
    test_template_dynamic_cast();
}
