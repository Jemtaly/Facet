#include <cassert>
#include <string>

#include <facet.hpp>
#include <facet/reflection.hpp>

struct[[= fct::spec]] IBase {
    void base() const;
};

struct[[= fct::spec]] IDerived : IBase {
    void derived() const;
};

struct GoodBaseModel {
    void base() const {}
};

struct BadBaseModel {};

struct GoodDerivedModel {
    void base() const {}
    void derived() const {}
};

struct BadDerivedModel {
    void derived() const {}
};

void test_implementation() {
    static_assert(fct::has_vtable_for<GoodBaseModel, IBase>);
    static_assert(!fct::has_vtable_for<BadBaseModel, IBase>);
    static_assert(fct::has_vtable_for<GoodDerivedModel, IDerived>);
    static_assert(!fct::has_vtable_for<BadDerivedModel, IDerived>);
}

struct[[= fct::spec]] INamed {
    std::string name() const;
};

struct NamedModel {
    std::string name_;

    std::string name() const {
        return name_;
    }
};

void test_named_interface() {
    NamedModel model{"example"};
    auto handle = fct::as_facets<INamed>(&model);
    fct::facets<INamed> named = handle;
    assert(named);
    assert(named->name() == "example");
    handle.get()->name_ = "updated";
    assert(named->name() == "updated");
}

template<class T>
struct[[= fct::spec]] IView {
    T get() const;
};

template<class T>
struct[[= fct::spec]] ICell : IView<T> {
    void set(T value);
};

template<class T>
struct Cell {
    T value;

    T get() const {
        return value;
    }

    void set(T next) {
        value = next;
    }
};

void test_template_interface() {
    Cell<std::string> text{"initial"};
    fct::facets<ICell<std::string>> cell = fct::as_facets<ICell<std::string>>(&text);
    fct::facets<IView<std::string>> view = cell;
    assert(view->get() == "initial");
    cell->set("updated");
    assert(view->get() == "updated");
}

struct[[= fct::spec]] ITop {
    int top() const;
};

struct[[= fct::spec]] ILeft : ITop {
    int left() const;
};

struct[[= fct::spec]] IRight : ITop {
    int right() const;
};

struct[[= fct::spec]] IBottom
    : ILeft
    , IRight {
    int bottom() const;
};

struct Model {
    int top() const {
        return 1;
    }

    int left() const {
        return 2;
    }

    int right() const {
        return 3;
    }

    int bottom() const {
        return 4;
    }
};

void test_upcast() {
    Model model;
    fct::facets<IBottom> bottom = fct::as_facets<IBottom>(&model);
    assert(bottom);
    assert(bottom->top() == 1);

    fct::facets<ILeft> left = bottom;
    assert(left);
    assert(left->top() == 1);

    fct::facets<IRight> right = bottom;
    assert(right);
    assert(right->top() == 1);

    fct::facets<ITop> top = bottom;
    assert(top);
    assert(top->top() == 1);
}

int main() {
    test_implementation();
    test_named_interface();
    test_template_interface();
    test_upcast();
}
