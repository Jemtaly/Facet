#include <array>
#include <cassert>

#include <proxy.hpp>
#include <proxy/reflection.hpp>

// Interface tags
struct[[= proxy::uuid("proxy.tests.manual.root")]] IManualRoot;
struct[[= proxy::uuid("proxy.tests.manual.reader")]] IManualReader;
struct[[= proxy::uuid("proxy.tests.manual.counter")]] IManualCounter;

struct RootDispatch {
    int (*root_value)(void *);
};

struct ReaderDispatch {
    int (*read)(void *);
};

struct CounterDispatch {
    int (*count)(void *);
};

namespace proxy {
template<>
struct vtable<IManualRoot> {
    struct type {
        RootDispatch const *self;
    };
};

template<>
struct direct_bases<IManualRoot> {
    static constexpr std::array<direct_base_t, 0> value = {};
};

template<>
struct direct_methods<IManualRoot> {
    static constexpr std::array<direct_method_t, 1> value = {
        direct_method("root_value", [](interface_handle<IManualRoot> handle) {
            return proxy::get<0>(handle.vtables)->self->root_value(handle.data);
        }),
    };
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
    static constexpr std::array<direct_base_t, 1> value = {
        direct_base<IManualRoot>([](vtable_t<IManualReader> const *table) {
            return &table->root;
        }),
    };
};

template<>
struct direct_methods<IManualReader> {
    static constexpr std::array<direct_method_t, 1> value = {
        direct_method("read", [](interface_handle<IManualReader> handle) {
            return proxy::get<0>(handle.vtables)->self->read(handle.data);
        }),
    };
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
    static constexpr std::array<direct_base_t, 1> value = {
        direct_base<IManualReader>([](vtable_t<IManualCounter> const *table) {
            return &table->reader;
        }),
    };
};

template<>
struct direct_methods<IManualCounter> {
    static constexpr std::array<direct_method_t, 1> value = {
        direct_method("count", [](interface_handle<IManualCounter> handle) {
            return proxy::get<0>(handle.vtables)->self->count(handle.data);
        }),
    };
};

template<typename Impl>
struct vtable_implements<Impl, IManualRoot> {
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

template<typename Impl>
struct vtable_implements<Impl, IManualReader> {
    static int read(void *data) {
        return static_cast<Impl *>(data)->value;
    }

    static constexpr ReaderDispatch dispatch{
        .read = read,
    };
    static constexpr vtable_t<IManualReader> table{
        .self = &dispatch,
        .root = vtable_implements<Impl, IManualRoot>::table,
    };
    static constexpr vtable_t<IManualReader> const *value = &table;
};

template<typename Impl>
struct vtable_implements<Impl, IManualCounter> {
    static int count(void *data) {
        return static_cast<Impl *>(data)->value;
    }

    static constexpr CounterDispatch dispatch{
        .count = count,
    };
    static constexpr vtable_t<IManualCounter> table{
        .self = &dispatch,
        .reader = vtable_implements<Impl, IManualReader>::table,
    };
    static constexpr vtable_t<IManualCounter> const *value = &table;
};
}

struct Counter {
    int value;
};

void test_manual_counter() {
    Counter counter{42};
    auto handle = proxy::as_handle<IManualCounter>(&counter);
    assert(handle->count() == 42);
    proxy::interface_handle<IManualReader> reader{handle};
    assert(reader->read() == 42);
    proxy::interface_handle<IManualRoot> root{reader};
    assert(root->root_value() == 42);

    proxy::interface_handle<IManualCounter> recast{reader};
    assert(recast);
    assert(recast->count() == 42);
}

struct IManualBase {};

namespace proxy {
template<>
struct interface_id<IManualBase> {
    static constexpr interface_id_t value = "proxy.tests.manual.base";
};

template<>
struct vtable<IManualBase> {
    struct type {
        int (*base)(void *data);
    };
};

template<>
struct direct_methods<IManualBase> {
    static constexpr std::array<direct_method_t, 1> value = {
        direct_method("base", [](interface_handle<IManualBase> handle) {
            return proxy::get<0>(handle.vtables)->base(handle.data);
        }),
    };
};

template<>
struct direct_bases<IManualBase> {
    static constexpr std::array<direct_base_t, 0> value = {};
};

template<typename Impl>
struct vtable_implements<Impl, IManualBase> {
    static int base(void *data) {
        return static_cast<Impl *>(data)->impl_base();
    }

    static constexpr vtable_t<IManualBase> table{
        .base = base,
    };
    static constexpr vtable_t<IManualBase> const *value = &table;
};
}

struct[[= proxy::spec]]
      [[= proxy::uuid("proxy.tests.manual.derived")]] IDerived : IManualBase {
    [[= proxy::adaptor([](auto impl) {
        return impl.get()->impl_derived();
    })]] int derived();
};

struct Derived {
    int impl_base() {
        return 12;
    }

    int impl_derived() {
        return 34;
    }
};

void test_manual_derived() {
    Derived derived;
    auto handle = proxy::as_handle<IDerived>(&derived);
    assert(handle->base() == 12);
    assert(handle->derived() == 34);
}

int main() {
    test_manual_counter();
    test_manual_derived();
}
