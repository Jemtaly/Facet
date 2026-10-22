#include <cassert>
#include <string>

#include <facet.hpp>
#include <facet/reflection.hpp>

struct[[= fct::spec]] INamed {
    std::string name() const;
};

struct NamedModel {
    std::string name_;

    std::string name() const {
        return name_;
    }
};

struct AnonymousModel {};

void test_basic_interface() {
    NamedModel model{"example"};
    auto handle = fct::as_facets<INamed>(&model);
    fct::facets<INamed> named = handle;
    assert(named);
    assert(named->name() == "example");
    handle.get()->name_ = "updated";
    assert(named->name() == "updated");

    static_assert(fct::has_vtable_for<NamedModel, INamed>);
    static_assert(!fct::has_vtable_for<AnonymousModel, INamed>);
}

struct[[= fct::spec]] IBase {
    int base() const;
};

struct[[= fct::spec]] IDerived : IBase {
    int derived() const;
};

struct GoodBaseModel {
    int base() const { return 1; }
};

struct BadBaseModel {};

struct GoodDerivedModel {
    int base() const { return 1; }
    int derived() const { return 2; }
};

struct BadDerivedModel {
    int derived() const { return 2; }
};

void test_basic_interface_inheritance() {
    GoodBaseModel good_base;
    auto good_base_handle = fct::as_facets<IBase>(&good_base);
    assert(good_base_handle);
    assert(good_base_handle->base() == 1);

    static_assert(fct::has_vtable_for<GoodBaseModel, IBase>);
    static_assert(!fct::has_vtable_for<BadBaseModel, IBase>);

    GoodDerivedModel good_derived;
    auto good_derived_handle = fct::as_facets<IDerived>(&good_derived);
    assert(good_derived_handle);
    assert(good_derived_handle->base() == 1);
    assert(good_derived_handle->derived() == 2);

    static_assert(fct::has_vtable_for<GoodDerivedModel, IDerived>);
    static_assert(!fct::has_vtable_for<BadDerivedModel, IDerived>);
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

void test_basic_interface_upcast() {
    Model model;
    fct::facets<IBottom> bottom = fct::as_facets<IBottom>(&model);
    assert(bottom);
    assert(bottom->top() == 1);
    assert(bottom->left() == 2);
    assert(bottom->right() == 3);
    assert(bottom->bottom() == 4);

    fct::facets<ILeft> left = bottom;
    assert(left);
    assert(left->top() == 1);
    assert(left->left() == 2);

    fct::facets<IRight> right = bottom;
    assert(right);
    assert(right->top() == 1);
    assert(right->right() == 3);

    fct::facets<ITop> top = bottom;
    assert(top);
    assert(top->top() == 1);
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

    static_assert(fct::has_vtable_for<Cell<int>, ICell<int>>);
    static_assert(fct::has_vtable_for<Cell<std::string>, ICell<std::string>>);
    static_assert(!fct::has_vtable_for<Cell<int>, ICell<std::string>>);
    static_assert(!fct::has_vtable_for<Cell<std::string>, ICell<int>>);
}

int main() {
    test_basic_interface();
    test_basic_interface_inheritance();
    test_basic_interface_upcast();
    test_template_interface();
}
