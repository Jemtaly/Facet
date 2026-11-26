#include <cassert>

#include <facet/macros.hpp>
#include <facet/reflection.hpp>

struct [[= fct::spec]] ICounter {
    int value() const;
};

FACET_METHOD(Add, add);

// A macro interface lists its reflected base among its entries.
struct IAdjustableCounter : fct::interface<IAdjustableCounter, ICounter, Add<void(int)>> {};

// A reflected interface uses ordinary C++ inheritance for its macro base.
struct [[= fct::spec]] IResettableCounter : IAdjustableCounter {
    void reset();
};

struct Counter {
    int count = 0;

    int value() const {
        return count;
    }

    void add(int step) {
        count += step;
    }

    void reset() {
        count = 0;
    }
};

int main() {
    Counter counter;
    auto resettable = fct::as_facets<IResettableCounter>(&counter);
    resettable->add(3);
    assert(resettable->value() == 3);

    fct::facets<IAdjustableCounter> adjustable = resettable;
    adjustable->add(4);
    assert(adjustable->value() == 7);

    fct::facets<ICounter> base = adjustable;
    resettable->reset();
    assert(base->value() == 0);
}
