#include <cassert>
#include <type_traits>

#include <proxy.hpp>
#include <proxy/memory.hpp>
#include <proxy/reflection.hpp>

struct ICounter {
    int count() const;
};

struct Tracked : proxy::enable_shared_from_this<Tracked> {
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

int main() {
    Tracked::alive = 0;
    Tracked::destroyed = 0;

    {
        auto shared = proxy::make_shared<Tracked, ICounter>();
        {
            auto copy = shared;
            assert(copy->count() == 42);
            proxy::interface_view<ICounter> view = copy;
            proxy::interface_holder<ICounter> retained{view};
            assert(retained->count() == 42);
        }
        assert(Tracked::alive == 1);

        auto recovered = proxy::recover_shared(shared.get_handle());
        proxy::interface_holder<ICounter> from_recovered{recovered};
        assert(from_recovered->count() == 42);

        proxy::interface_holder<ICounter> erased = std::move(shared);
        assert(!shared);
        assert(erased->count() == 42);
    }
    assert(Tracked::alive == 0);
    assert(Tracked::destroyed == 1);

    {
        auto unique = proxy::make_unique<Tracked, ICounter>();
        static_assert(!std::is_copy_constructible_v<decltype(unique)>);
        static_assert(std::is_move_constructible_v<decltype(unique)>);
        auto moved = std::move(unique);
        assert(!unique);
        proxy::interface_owner<ICounter> erased = std::move(moved);
        assert(!moved);
        assert(erased->count() == 42);
    }
    assert(Tracked::alive == 0);
    assert(Tracked::destroyed == 2);

    {
        Tracked stack_object;
        auto borrowed = proxy::as_handle<ICounter>(&stack_object);
        assert(borrowed->count() == 42);
    }
    assert(Tracked::alive == 0);
    assert(Tracked::destroyed == 3);
}
