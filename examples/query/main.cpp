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

void test_query_interface() {
    Model model;
    auto initial = fct::as_facets<IRoot, IExtra>(&model);
    fct::facets<IRoot> root = initial;

    static_assert(!std::is_convertible_v<
                  fct::facets<IExtra>,
                  fct::facets<IRoot>>);
    fct::facets<IExtra> extra{root};
    assert(extra);
    assert(extra->extra() == 20);

    static_assert(!std::is_convertible_v<
                  fct::facets<IAbsent>,
                  fct::facets<IRoot>>);
    fct::facets<IAbsent> absent{root};
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

void test_query_template_interface() {
    TemplateModel model;
    auto handle = fct::as_facets<ITemplate<int>>(&model);
    fct::opaque root = handle;

    fct::facets<ITemplate<int>> template_handle{root};
    assert(template_handle);
    assert(template_handle->value() == 42);

    fct::facets<ITemplate<double>> wrong_template_handle{root};
    assert(!wrong_template_handle);
}

int main() {
    test_query_interface();
    test_query_template_interface();
}
