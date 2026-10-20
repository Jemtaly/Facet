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

void test_overloaded_implementation_methods() {
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

void test_same_name_automatic_dispatch() {
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

void test_manual_dispatch_for_name_conflicts() {
    ConflictModel model;
    auto handle = fct::as_facets<ILeft, IRight>(&model);
    assert(handle.as<ILeft>()->value() == 1);
    assert(handle.as<IRight>()->value() == 2);
}

struct[[= fct::spec]] ISelect {
    int select(int value) const;
};

struct ExactStaticWins {
    int select(int value) const {
        return value + 10;
    }

    static int select(fct::typed_facets<ExactStaticWins, ISelect>, int value) {
        return value + 20;
    }
};

struct ExactMemberWins {
    int select(int value) const {
        return value + 30;
    }

    static int select(fct::typed_facets<ExactMemberWins, ISelect>, double value) {
        return static_cast<int>(value) + 40;
    }
};

struct CompatibleStaticWins {
    int select(double value) const {
        return static_cast<int>(value) + 50;
    }

    static int select(fct::typed_facets<CompatibleStaticWins, ISelect>, double value) {
        return value + 60;
    }
};

struct CompatibleMemberWins {
    int select(double value) const {
        return static_cast<int>(value) + 70;
    }
};

void test_implementation_priority() {
    ExactStaticWins model_a;
    assert(fct::as_facets<ISelect>(&model_a)->select(1) == 21);

    ExactMemberWins model_b;
    assert(fct::as_facets<ISelect>(&model_b)->select(1) == 31);

    CompatibleStaticWins model_c;
    assert(fct::as_facets<ISelect>(&model_c)->select(1) == 61);

    CompatibleMemberWins model_d;
    assert(fct::as_facets<ISelect>(&model_d)->select(1) == 71);
}

int main() {
    test_overloaded_implementation_methods();
    test_same_name_automatic_dispatch();
    test_manual_dispatch_for_name_conflicts();
    test_implementation_priority();
}
