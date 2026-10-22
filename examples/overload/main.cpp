#include <cassert>
#include <string>

#include <facet.hpp>
#include <facet/reflection.hpp>

struct[[= fct::spec]] ICalculator {
    int calculate(int value) const;
    double calculate(double value) const;
};

struct Calculator {
    int calculate(int value) const {
        return value * 3;
    }

    double calculate(double value) const {
        return value * 2.5;
    }
};

void test_overload_within_single_interface() {
    Calculator calculator;
    auto handle = fct::as_facets<ICalculator>(&calculator);
    assert(handle->calculate(2) == 6);
    assert(handle->calculate(2.0) == 5.0);
}

struct[[= fct::spec]] ITextChannel {
    std::string send(int index) const;
};

struct[[= fct::spec]] IIndexChannel {
    double send(double index) const;
};

struct Channel {
    std::string send(int index) const {
        return std::to_string(index);
    }

    double send(double index) const {
        return index * 0.5;
    }
};

void test_overload_between_different_interfaces() {
    Channel channel;
    auto handle = fct::as_facets<ITextChannel, IIndexChannel>(&channel);
    assert(handle->send(2) == "2");
    assert(handle->send(2.0) == 1.0);
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
