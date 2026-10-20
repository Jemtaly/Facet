#include <cassert>
#include <type_traits>

#include <facet.hpp>
#include <facet/reflection.hpp>

struct[[= fct::spec]]
      [[= fct::local_id]] IRoot {
    int root() const;
};

struct[[= fct::spec]]
      [[= fct::local_id]] IExtra {
    int extra() const;
};

struct[[= fct::spec]]
      [[= fct::local_id]] IAbsent {
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
    auto initial = fct::as_handle<IRoot, IExtra>(&model);
    fct::interface_handle<IRoot> root = initial;

    static_assert(!std::is_convertible_v<
                  fct::interface_handle<IExtra>,
                  fct::interface_handle<IRoot>>);
    fct::interface_handle<IExtra> extra{root};
    assert(extra);
    assert(extra->extra() == 20);

    static_assert(!std::is_convertible_v<
                  fct::interface_handle<IAbsent>,
                  fct::interface_handle<IRoot>>);
    fct::interface_handle<IAbsent> absent{root};
    assert(!absent);
}

template<typename T>
struct[[= fct::spec]]
      [[= fct::local_id]] ITemplate {
    T value() const;
};

struct TemplateModel {
    int value() const {
        return 42;
    }
};

void test_template_dynamic_cast() {
    TemplateModel model;
    auto handle = fct::as_handle<ITemplate<int>>(&model);
    fct::data_handle root = handle;

    fct::interface_handle<ITemplate<int>> template_handle{root};
    assert(template_handle);
    assert(template_handle->value() == 42);

    fct::interface_handle<ITemplate<double>> wrong_template_handle{root};
    assert(!wrong_template_handle);
}

int main() {
    test_dynamic_cast();
    test_template_dynamic_cast();
}
