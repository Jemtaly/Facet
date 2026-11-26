#include <cassert>
#include <typeinfo>

#include <facet.hpp>
#include <facet/reflection.hpp>

namespace manual_names {
// An unnamed forwarder keeps its class name out of facade member lookup.
struct root_value_name {
    template<class Slot>
    struct bind_def {
        using type = struct {
            template<class... Args>
                requires std::invocable<Slot, Args...>
            decltype(auto) root_value(Args &&...args) const {
                return Slot(this)(std::forward<Args>(args)...);
            }
        };
    };

    template<class Slot>
    using bind = typename bind_def<Slot>::type;
};

struct read_name {
    template<class Slot>
    struct bind_def {
        using type = struct {
            template<class... Args>
                requires std::invocable<Slot, Args...>
            decltype(auto) read(Args &&...args) const {
                return Slot(this)(std::forward<Args>(args)...);
            }
        };
    };

    template<class Slot>
    using bind = typename bind_def<Slot>::type;
};

struct count_name {
    template<class Slot>
    struct bind_def {
        using type = struct {
            template<class... Args>
                requires std::invocable<Slot, Args...>
            decltype(auto) count(Args &&...args) const {
                return Slot(this)(std::forward<Args>(args)...);
            }
        };
    };

    template<class Slot>
    using bind = typename bind_def<Slot>::type;
};

struct base_name {
    template<class Slot>
    struct bind_def {
        using type = struct {
            template<class... Args>
                requires std::invocable<Slot, Args...>
            decltype(auto) base(Args &&...args) const {
                return Slot(this)(std::forward<Args>(args)...);
            }
        };
    };

    template<class Slot>
    using bind = typename bind_def<Slot>::type;
};

struct value_name {
    template<class Slot>
    struct bind_def {
        using type = struct {
            template<class... Args>
                requires std::invocable<Slot, Args...>
            decltype(auto) value(Args &&...args) const {
                return Slot(this)(std::forward<Args>(args)...);
            }
        };
    };

    template<class Slot>
    using bind = typename bind_def<Slot>::type;
};

struct bind_name {
    template<class Slot>
    struct bind_def {
        using type = struct {
            template<class... Args>
                requires std::invocable<Slot, Args...>
            decltype(auto) bind(Args &&...args) const {
                return Slot(this)(std::forward<Args>(args)...);
            }
        };
    };

    template<class Slot>
    using bind = typename bind_def<Slot>::type;
};

struct type_name {
    template<class Slot>
    struct bind_def {
        using type = struct {
            template<class... Args>
                requires std::invocable<Slot, Args...>
            decltype(auto) type(Args &&...args) const {
                return Slot(this)(std::forward<Args>(args)...);
            }
        };
    };

    template<class Slot>
    using bind = typename bind_def<Slot>::type;
};
}

struct[[= fct::id("facet.tests.manual.root")]] IManualRoot;
struct[[= fct::id("facet.tests.manual.reader")]] IManualReader;
struct[[= fct::id("facet.tests.manual.counter")]] IManualCounter;

struct RootDispatch {
    int (*root_value)(void *);
};

struct ReaderDispatch {
    int (*read)(void *);
};

struct CounterDispatch {
    int (*count)(void *);
};

namespace fct {
template<>
struct vtable<IManualRoot> {
    struct type {
        RootDispatch const *self;
    };
};

template<>
struct direct_bases<IManualRoot> {
    using type = type_list<>;
};

template<>
struct direct_methods<IManualRoot> {
    using type = type_list<
        direct_method<manual_names::root_value_name, [](facets<IManualRoot> handle) {
            return handle.template vtable_at<0>()->self->root_value(handle.get());
        }>>;
};

template<class Impl>
struct vtable_for<Impl, IManualRoot> {
    static int root_value(void *data) {
        return static_cast<Impl *>(data)->value;
    }

    static constexpr RootDispatch dispatch{
        .root_value = root_value,
    };
    static constexpr vtable_t<IManualRoot> table{
        .self = &dispatch,
    };
    static constexpr vtable_t<IManualRoot> const *value = &table;
};

template<>
struct vtable<IManualReader> {
    struct type {
        ReaderDispatch const *self;
        vtable_t<IManualRoot> root;
    };
};

template<>
struct direct_bases<IManualReader> {
    using type = type_list<
        direct_base<IManualRoot, [](vtable_t<IManualReader> const *table) {
            return &table->root;
        }>>;
};

template<>
struct direct_methods<IManualReader> {
    using type = type_list<
        direct_method<manual_names::read_name, [](facets<IManualReader> handle) {
            return handle.template vtable_at<0>()->self->read(handle.get());
        }>>;
};

template<class Impl>
struct vtable_for<Impl, IManualReader> {
    static int read(void *data) {
        return static_cast<Impl *>(data)->value;
    }

    static constexpr ReaderDispatch dispatch{
        .read = read,
    };
    static constexpr vtable_t<IManualReader> table{
        .self = &dispatch,
        .root = vtable_for<Impl, IManualRoot>::table,
    };
    static constexpr vtable_t<IManualReader> const *value = &table;
};

template<>
struct vtable<IManualCounter> {
    struct type {
        CounterDispatch const *self;
        vtable_t<IManualReader> reader;
    };
};

template<>
struct direct_bases<IManualCounter> {
    using type = type_list<
        direct_base<IManualReader, [](vtable_t<IManualCounter> const *table) {
            return &table->reader;
        }>>;
};

template<>
struct direct_methods<IManualCounter> {
    using type = type_list<
        direct_method<manual_names::count_name, [](facets<IManualCounter> handle) {
            return handle.template vtable_at<0>()->self->count(handle.get());
        }>>;
};

template<class Impl>
struct vtable_for<Impl, IManualCounter> {
    static int count(void *data) {
        return static_cast<Impl *>(data)->value;
    }

    static constexpr CounterDispatch dispatch{
        .count = count,
    };
    static constexpr vtable_t<IManualCounter> table{
        .self = &dispatch,
        .reader = vtable_for<Impl, IManualReader>::table,
    };
    static constexpr vtable_t<IManualCounter> const *value = &table;
};
}

struct Counter {
    int value;
};

void test_manual_interface() {
    Counter counter{42};
    auto handle = fct::as_facets<IManualCounter>(&counter);
    assert(handle->count() == 42);

    fct::facets<IManualReader> reader{handle};
    assert(reader->read() == 42);

    fct::facets<IManualRoot> root{reader};
    assert(root->root_value() == 42);

    fct::facets<IManualCounter> recast{reader};
    assert(recast);
    assert(recast->count() == 42);
}

struct IManualBase {};

namespace fct {
template<>
struct interface_id<IManualBase> {
    static constexpr interface_id_t value = "facet.tests.manual.base";
};

template<>
struct vtable<IManualBase> {
    struct type {
        int (*base)(void *data);
    };
};

template<>
struct direct_methods<IManualBase> {
    using type = type_list<
        direct_method<manual_names::base_name, [](facets<IManualBase> handle) {
            return handle.template vtable_at<0>()->base(handle.get());
        }>>;
};

template<>
struct direct_bases<IManualBase> {
    using type = type_list<>;
};

template<class Impl>
struct vtable_for<Impl, IManualBase> {
    static int base(void *data) {
        return static_cast<Impl *>(data)->base();
    }

    static constexpr vtable_t<IManualBase> table{
        .base = base,
    };
    static constexpr vtable_t<IManualBase> const *value = &table;
};
}

struct[[= fct::spec]]
      [[= fct::id("facet.tests.manual.derived")]] IDerived : IManualBase {
    int derived();
};

struct Derived {
    int base() {
        return 12;
    }

    int derived() {
        return 34;
    }
};

void test_manual_interface_inheritance() {
    Derived derived;
    auto handle = fct::as_facets<IDerived>(&derived);
    assert(handle->base() == 12);
    assert(handle->derived() == 34);
}

template<class T>
struct IManualValue;

namespace fct {
template<class T>
struct interface_id<IManualValue<T>> {
    static constexpr interface_id_t value =
        static_string::make(std::meta::display_string_of(^^IManualValue<T>)).data;
};

template<class T>
struct vtable<IManualValue<T>> {
    struct type {
        T (*value)(void *);
        void (*bind)(void *, T);
        std::type_info const &(*model_type)();
    };
};

template<class T>
struct direct_bases<IManualValue<T>> {
    using type = type_list<>;
};

template<class T>
struct direct_methods<IManualValue<T>> {
    using type = type_list<
        direct_method<manual_names::value_name, [](facets<IManualValue<T>> handle) {
            return handle.template vtable_at<0>()->value(handle.get());
        }>,
        direct_method<manual_names::bind_name, [](facets<IManualValue<T>> handle, T value) {
            handle.template vtable_at<0>()->bind(handle.get(), std::move(value));
        }>,
        direct_method<manual_names::type_name, [](facets<IManualValue<T>> handle) -> std::type_info const & {
            return handle.template vtable_at<0>()->model_type();
        }>>;
};

template<class Impl, class T>
struct vtable_for<Impl, IManualValue<T>> {
    static T invoke(void *data) {
        return static_cast<Impl *>(data)->value;
    }

    static constexpr vtable_t<IManualValue<T>> table{
        .value = invoke,
        .bind = [](void *data, T value) { static_cast<Impl *>(data)->value = std::move(value); },
        .model_type = []() -> std::type_info const & { return typeid(Impl); },
    };
    static constexpr vtable_t<IManualValue<T>> const *value = &table;
};
}

struct ValueModel {
    int value;
};

void test_manual_template_interface() {
    ValueModel model{56};
    auto handle = fct::as_facets<IManualValue<int>>(&model);
    fct::opaque data = handle;

    fct::facets<IManualValue<int>> value{data};
    assert(value);
    assert(value->value() == 56);
    assert(value->type() == typeid(ValueModel));
    value->bind(78);
    assert(model.value == 78);

    fct::facets<IManualValue<double>> wrong_value{data};
    assert(!wrong_value);
}

int main() {
    test_manual_interface();
    test_manual_interface_inheritance();
    test_manual_template_interface();
}
