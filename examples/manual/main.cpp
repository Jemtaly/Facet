#include <array>
#include <cassert>

#include <proxy.hpp>

// Interface tags
struct IManualRoot;
struct IManualReader;
struct IManualCounter;

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
struct interface_id<IManualRoot> {
    static constexpr interface_id_t value = "proxy.tests.manual.root";
};

template<>
struct interface_id<IManualReader> {
    static constexpr interface_id_t value = "proxy.tests.manual.reader";
};

template<>
struct interface_id<IManualCounter> {
    static constexpr interface_id_t value = "proxy.tests.manual.counter";
};

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
            return std::get<0>(handle.vtables)->self->root_value(handle.data);
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
            return std::get<0>(handle.vtables)->self->read(handle.data);
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
            return std::get<0>(handle.vtables)->self->count(handle.data);
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

int main() {
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
