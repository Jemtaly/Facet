#include <cassert>
#include <string>

#include <proxy.hpp>
#include <proxy/reflection.hpp>

struct[[= proxy::spec]] IBase {
    void base() const;
};

struct[[= proxy::spec]] IDerived : IBase {
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
    static_assert(proxy::implements<GoodBaseModel, IBase>);
    static_assert(!proxy::implements<BadBaseModel, IBase>);
    static_assert(proxy::implements<GoodDerivedModel, IDerived>);
    static_assert(!proxy::implements<BadDerivedModel, IDerived>);
}

struct[[= proxy::spec]] INamed {
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
    auto handle = proxy::as_handle<INamed>(&model);
    proxy::interface_handle<INamed> named = handle;
    assert(named);
    assert(named->name() == "example");
    handle.get()->name_ = "updated";
    assert(named->name() == "updated");
}

template<class T>
struct[[= proxy::spec]] IView {
    T get() const;
};

template<class T>
struct[[= proxy::spec]] ICell : IView<T> {
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
    proxy::interface_handle<ICell<std::string>> cell = proxy::as_handle<ICell<std::string>>(&text);
    proxy::interface_handle<IView<std::string>> view = cell;
    assert(view->get() == "initial");
    cell->set("updated");
    assert(view->get() == "updated");
}

struct[[= proxy::spec]] ITop {
    int top() const;
};

struct[[= proxy::spec]] ILeft : ITop {
    int left() const;
};

struct[[= proxy::spec]] IRight : ITop {
    int right() const;
};

struct[[= proxy::spec]] IBottom
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
    proxy::interface_handle<IBottom> bottom = proxy::as_handle<IBottom>(&model);
    assert(bottom);
    assert(bottom->top() == 1);

    proxy::interface_handle<ILeft> left = bottom;
    assert(left);
    assert(left->top() == 1);

    proxy::interface_handle<IRight> right = bottom;
    assert(right);
    assert(right->top() == 1);

    proxy::interface_handle<ITop> top = bottom;
    assert(top);
    assert(top->top() == 1);
}

int main() {
    test_implementation();
    test_named_interface();
    test_template_interface();
    test_upcast();
}
