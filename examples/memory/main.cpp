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
            fct::weak_facets<ICounter> view = copy;
            fct::shared_facets<ICounter> retained{view};
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
        auto borrowed = fct::as_facets<ICounter>(&stack_object);
        assert(borrowed->count() == 42);
    }
    assert(Tracked::alive == 0);
    assert(Tracked::destroyed == 3);
}

struct[[= fct::spec]] IRecoverable {
    fct::shared_facets<IRecoverable> retain() const;
};

struct Recoverable : fct::enable_shared_from_this<Recoverable> {
    static inline int alive = 0;
    static inline int destroyed = 0;

    Recoverable() {
        ++alive;
    }

    ~Recoverable() {
        --alive;
        ++destroyed;
    }

    static fct::shared_facets<IRecoverable> retain(fct::typed_facets<Recoverable, IRecoverable> self) {
        return fct::retain_shared(self);
    }
};

void test_recoverable() {
    Recoverable::alive = 0;
    Recoverable::destroyed = 0;

    {
        auto object = fct::make_shared<Recoverable, IRecoverable>();
        {
            fct::shared_facets<IRecoverable> retained = object->retain();
            assert(retained);
        }
        assert(Recoverable::alive == 1);
    }
    assert(Recoverable::alive == 0);
    assert(Recoverable::destroyed == 1);
}

int main() {
    test_tracked_lifetime();
    test_recoverable();
}
