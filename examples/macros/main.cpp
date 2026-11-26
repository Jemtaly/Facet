#include <cassert>

#include <facet/macros.hpp>

FACET_METHOD(Increment, increment);

struct ICounter : fct::interface<ICounter, Increment<int()>> {};

using IncrementBy = Increment<int(int)>::dispatch_via<[](auto self, int step) {
    return self.get()->add(step);
}>;

struct IAdjustableCounter : fct::interface<IAdjustableCounter, ICounter, IncrementBy> {};

struct Counter {
    int value = 0;

    int increment() {
        return add(1);
    }

    int add(int step) {
        return value += step;
    }
};

int main() {
    Counter counter;
    auto handle = fct::as_facets<IAdjustableCounter>(&counter);
    assert(handle->increment() == 1);
    assert(handle->increment(4) == 5);

    fct::facets<ICounter> base = handle;
    assert(base->increment() == 6);
}
