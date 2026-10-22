#include <cassert>
#include <string>

#include <facet.hpp>
#include <facet/reflection.hpp>

struct[[= fct::spec]] ICalculator {
    int calculate() const;
    int calculate(int value) const;
};

struct Calculator {
    int calculate() const {
        return 3;
    }

    int calculate(int value) const {
        return value * 4;
    }
};

void test_overload_within_single_interface() {
    Calculator calculator;
    auto handle = fct::as_facets<ICalculator>(&calculator);
    assert(handle->calculate() == 3);
    assert(handle->calculate(5) == 20);
}

struct[[= fct::spec]] ITextChannel {
    std::string send() const;
};

struct[[= fct::spec]] IIndexChannel {
    int send(int index) const;
};

struct Channel {
    std::string send() const {
        return "text";
    }

    int send(int index) const {
        return index * 3;
    }
};

void test_overload_between_different_interfaces() {
    Channel channel;
    auto handle = fct::as_facets<ITextChannel, IIndexChannel>(&channel);
    assert(handle->send() == "text");
    assert(handle->send(7) == 21);
}

struct[[= fct::spec]] ILeft {
    int value() const;
};

struct[[= fct::spec]] IRight {
    int value() const;
};

struct ConflictModel {
    static int value(fct::typed_facets<ConflictModel, ILeft>) {
        return 1;
    }

    static int value(fct::typed_facets<ConflictModel, IRight>) {
        return 2;
    }
};

void test_conflict_between_different_interfaces() {
    ConflictModel model;
    auto handle = fct::as_facets<ILeft, IRight>(&model);
    assert(handle.as<ILeft>()->value() == 1);
    assert(handle.as<IRight>()->value() == 2);
}

int main() {
    test_overload_within_single_interface();
    test_overload_between_different_interfaces();
    test_conflict_between_different_interfaces();
}
