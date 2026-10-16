#include <cassert>
#include <string>

#include <proxy.hpp>
#include <proxy/reflection.hpp>

struct ICalculator {
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
    auto handle = proxy::as_handle<ICalculator>(&calculator);
    assert(handle->calculate() == 3);
    assert(handle->calculate(5) == 20);
}

struct ITextChannel {
    std::string send() const;
};

struct IIndexChannel {
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
    auto handle = proxy::as_handle<ITextChannel, IIndexChannel>(&channel);
    assert(handle->send() == "text");
    assert(handle->send(7) == 21);
}

struct ILeft {
    int value() const;
};

struct IRight {
    int value() const;
};

struct ConflictModel {
    static int value(proxy::object_handle<ConflictModel, ILeft>) {
        return 1;
    }

    static int value(proxy::object_handle<ConflictModel, IRight>) {
        return 2;
    }
};

void test_manual_dispatch_for_name_conflicts() {
    ConflictModel model;
    auto handle = proxy::as_handle<ILeft, IRight>(&model);
    assert(handle.as<ILeft>()->value() == 1);
    assert(handle.as<IRight>()->value() == 2);
}

struct ISelect {
    int select(int value) const;
};

struct ExactStaticWins {
    int select(int value) const {
        return value + 10;
    }

    static int select(proxy::object_handle<ExactStaticWins, ISelect>, int value) {
        return value + 20;
    }
};

struct ExactMemberWins {
    int select(int value) const {
        return value + 30;
    }

    static int select(proxy::object_handle<ExactMemberWins, ISelect>, double value) {
        return static_cast<int>(value) + 40;
    }
};

struct CompatibleStaticWins {
    int select(double value) const {
        return static_cast<int>(value) + 50;
    }

    static int select(proxy::object_handle<CompatibleStaticWins, ISelect>, double value) {
        return value + 60;
    }
};

struct CompatibleMemberWins {
    int select(double value) const {
        return static_cast<int>(value) + 70;
    }
};

void test_implementation_priority() {
    {
        ExactStaticWins model;
        assert(proxy::as_handle<ISelect>(&model)->select(1) == 21);
    }

    {
        ExactMemberWins model;
        assert(proxy::as_handle<ISelect>(&model)->select(1) == 31);
    }

    {
        CompatibleStaticWins model;
        assert(proxy::as_handle<ISelect>(&model)->select(1) == 61);
    }

    {
        CompatibleMemberWins model;
        assert(proxy::as_handle<ISelect>(&model)->select(1) == 71);
    }
}

int main() {
    test_overloaded_implementation_methods();
    test_same_name_automatic_dispatch();
    test_manual_dispatch_for_name_conflicts();
    test_implementation_priority();
}
