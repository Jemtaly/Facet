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
            fct::borrowed_facets<ICounter> view = copy;
            fct::shared_facets<ICounter> retained = view;
            assert(retained->count() == 42);
        }
        assert(Tracked::alive == 1);

        fct::shared_facets<ICounter> erased = std::move(shared);
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
        fct::unique_facets<ICounter> erased = std::move(moved);
        assert(!moved);
        assert(erased->count() == 42);
    }
    assert(Tracked::alive == 0);
    assert(Tracked::destroyed == 2);

    {
        Tracked stack_object;
        auto ref = fct::as_facets<ICounter>(&stack_object);
        assert(ref->count() == 42);
    }
    assert(Tracked::alive == 0);
    assert(Tracked::destroyed == 3);
}

struct[[= fct::spec]] IRetainable {
    fct::shared_facets<IRetainable> retain() const;
    void invoke(std::function<void(fct::borrowed_facets<IRetainable>)>) const;
};

struct Retainable : fct::enable_shared_from_self<Retainable> {
    static inline int alive = 0;
    static inline int destroyed = 0;

    Retainable() {
        ++alive;
    }

    ~Retainable() {
        --alive;
        ++destroyed;
    }

    static fct::shared_facets<IRetainable> retain(fct::typed_facets<Retainable, IRetainable> self) {
        return fct::shared_from(self);
    }

    static void invoke(fct::typed_facets<Retainable, IRetainable> self, std::function<void(fct::borrowed_facets<IRetainable>)> callback) {
        callback(fct::borrowed_from(self));
    }
};

void test_shared_from_self() {
    Retainable::alive = 0;
    Retainable::destroyed = 0;

    {
        auto object = fct::make_shared<Retainable, IRetainable>();
        {
            fct::shared_facets<IRetainable> retained = object->retain();
            assert(retained);

            object->invoke([](fct::borrowed_facets<IRetainable> retained) {
                assert(retained);
            });
        }
        assert(Retainable::alive == 1);
    }
    assert(Retainable::alive == 0);
    assert(Retainable::destroyed == 1);
}

int main() {
    test_tracked_lifetime();
    test_shared_from_self();
}
