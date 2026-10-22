#include <cassert>

#include <facet.hpp>
#include <facet/reflection.hpp>

struct [[= fct::spec]] ISelect {
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

void test_default_dispatcher() {
    ExactStaticWins model_a;
    assert(fct::as_facets<ISelect>(&model_a)->select(1) == 21);

    ExactMemberWins model_b;
    assert(fct::as_facets<ISelect>(&model_b)->select(1) == 31);

    CompatibleStaticWins model_c;
    assert(fct::as_facets<ISelect>(&model_c)->select(1) == 61);

    CompatibleMemberWins model_d;
    assert(fct::as_facets<ISelect>(&model_d)->select(1) == 71);
}

struct [[= fct::spec]] IShippingQuote {
    double quote_dollars_per_kilogram(double weight_kilograms) const;
    [[= fct::dispatch_via([](auto self, double weight_grams) -> double {
        if constexpr (requires {
            { self.get()->quote_cents_per_gram(weight_grams) } -> std::convertible_to<double>;
        }) {
            return self.get()->quote_cents_per_gram(weight_grams);
        } else {
            return self->quote_dollars_per_kilogram(weight_grams / 1000) * 100;
        }
    })]] double
    quote_cents_per_gram(double weight_grams) const;
};

struct CentRateService {
    double quote_dollars_per_kilogram(double weight_kilograms) const {
        return weight_kilograms;
    }

    double quote_cents_per_gram(double weight_grams) const {
        return weight_grams / 10;
    }
};

struct DollarRateService {
    double quote_dollars_per_kilogram(double weight_kilograms) const {
        return weight_kilograms * 1.25;
    }
};

void test_custom_dispatcher() {
    CentRateService cent_rate_service;
    auto cent_quote = fct::as_facets<IShippingQuote>(&cent_rate_service);
    assert(cent_quote->quote_dollars_per_kilogram(2) == 2);
    assert(cent_quote->quote_cents_per_gram(2000) == 200);

    DollarRateService dollar_rate_service;
    auto dollar_quote = fct::as_facets<IShippingQuote>(&dollar_rate_service);
    assert(dollar_quote->quote_dollars_per_kilogram(2) == 2.5);
    assert(dollar_quote->quote_cents_per_gram(2000) == 250);
}

template<typename Sig>
struct IFunction;

template<typename Ret, typename... Args>
struct [[= fct::spec]] IFunction<Ret(Args...)> {
    [[= fct::dispatch_via([](auto self, Args... args) -> Ret
        requires requires {
            { (*self.get())(args...) } -> std::convertible_to<Ret>;
        }
    {
        return (*self.get())(args...);
    })]]
    Ret invoke(Args... args) const;
};

void test_custom_dispatcher_with_requires() {
    auto lambda = [](int x) -> int {
        return x + 5;
    };
    auto func = fct::as_facets<IFunction<int(int)>>(&lambda);
    assert(func->invoke(10) == 15);

    static_assert(!fct::has_vtable_for<decltype(lambda), IFunction<void()>>);
}

int main() {
    test_default_dispatcher();
    test_custom_dispatcher();
    test_custom_dispatcher_with_requires();
}
