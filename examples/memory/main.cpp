#include <cassert>
#include <type_traits>

#include <facet.hpp>
#include <facet/memory.hpp>
#include <facet/reflection.hpp>

struct[[= fct::spec]] ICounter {
    int count() const;
};

struct Tracked {
    static inline int alive = 0;
    static inline int destroyed = 0;

    Tracked() {
        ++alive;
    }

    ~Tracked() {
        --alive;
        ++destroyed;
    }

    int count() const {
        return 42;
    }
};

void test_tracked_lifetime() {
    Tracked::alive = 0;
    Tracked::destroyed = 0;

    {
        auto shared = fct::make_shared<Tracked, ICounter>();
        {
            auto copy = shared;
            assert(copy->count() == 42);
            fct::interface_view<ICounter> view = copy;
            fct::interface_holder<ICounter> retained{view};
            assert(retained->count() == 42);
        }
        assert(Tracked::alive == 1);

        fct::interface_holder<ICounter> erased = std::move(shared);
        assert(!shared);
        assert(erased->count() == 42);
    }
    assert(Tracked::alive == 0);
    assert(Tracked::destroyed == 1);

    {
        auto unique = fct::make_unique<Tracked, ICounter>();
        static_assert(!std::is_copy_constructible_v<decltype(unique)>);
        static_assert(std::is_move_constructible_v<decltype(unique)>);
        auto moved = std::move(unique);
        assert(!unique);
        fct::interface_owner<ICounter> erased = std::move(moved);
        assert(!moved);
        assert(erased->count() == 42);
    }
    assert(Tracked::alive == 0);
    assert(Tracked::destroyed == 2);

    {
        Tracked stack_object;
        auto borrowed = fct::as_handle<ICounter>(&stack_object);
        assert(borrowed->count() == 42);
    }
    assert(Tracked::alive == 0);
    assert(Tracked::destroyed == 3);
}

struct[[= fct::spec]] IRecoverable {
    fct::interface_holder<IRecoverable> retain() const;
};

struct Recoverable : fct::enable_shared_from_this<Recoverable> {
    static fct::interface_holder<IRecoverable> retain(fct::object_handle<Recoverable, IRecoverable> self) {
        return fct::recover_shared(self);
    }
};

void test_recoverable() {
    auto object = fct::make_shared<Recoverable, IRecoverable>();
    fct::interface_holder<IRecoverable> retained = object->retain();
    assert(retained);
}

int main() {
    test_tracked_lifetime();
    test_recoverable();
}
