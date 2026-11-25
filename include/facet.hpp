#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

#define FACET_TYPE_INFO_VERSION 0

// ============================================================================
// Basic utilities
// ============================================================================

namespace fct {
template<class... Ts>
struct type_list {
private:
    template<std::size_t I, class List>
    struct element;

    template<std::size_t I, class Head, class... Tail>
    struct element<I, type_list<Head, Tail...>> : element<I - 1, type_list<Tail...>> {};

    template<class Head, class... Tail>
    struct element<0, type_list<Head, Tail...>> {
        using type = Head;
    };

public:
    static constexpr std::size_t size = sizeof...(Ts);

    template<std::size_t I>
    using at = typename element<I, type_list>::type;
};

template<class Initial, class... Lists>
struct concat;

template<class... Ts>
struct concat<
    type_list<Ts...>> {
    using type = type_list<Ts...>;
};

template<class... Ls, class... Rs, class... Rest>
struct concat<
    type_list<Ls...>,
    type_list<Rs...>, Rest...>
    : concat<
          type_list<Ls..., Rs...>,
          Rest...> {};

template<class... Lists>
using concat_t = typename concat<type_list<>, Lists...>::type;

template<class... Ts>
struct tuple;

template<>
struct tuple<> {
    static constexpr std::size_t size = 0;
};

template<class Head, class... Tail>
struct tuple<Head, Tail...> {
    static constexpr std::size_t size = 1 + sizeof...(Tail);
    Head head;
    [[no_unique_address]] tuple<Tail...> tail;
};

template<std::size_t I, class T>
constexpr decltype(auto) get(T &&t) noexcept {
    if constexpr (I == 0) {
        return (std::forward<T>(t).head);
    } else {
        return fct::get<I - 1>(std::forward<T>(t).tail);
    }
}

template<class F, class T>
constexpr decltype(auto) apply(F &&f, T &&t) {
    return [&]<std::size_t... I>(std::index_sequence<I...>) -> decltype(auto) {
        return std::invoke(std::forward<F>(f), fct::get<I>(std::forward<T>(t))...);
    }(std::make_index_sequence<std::remove_cvref_t<T>::size>{});
}
}

// ============================================================================
// Interface protocol
//
// These templates are the non-reflective protocol consumed by the rest of the
// library. Their defaults are generated from an interface specification, but
// users may specialize them for interfaces that cannot or should not use the
// automatic reflection path.
// ============================================================================

namespace fct {
template<class Face>
struct vtable;

template<class Face>
using vtable_t = typename vtable<Face>::type;

template<class Name, auto Invoker>
struct direct_method {
    using name = Name;
    static constexpr auto invoker = Invoker;
};

template<class Face>
struct direct_methods;

template<class Face>
using direct_methods_t = typename direct_methods<Face>::type;

template<class Base, auto Converter>
struct direct_base {
    using type = Base;
    static constexpr auto converter = Converter;
};

template<class Face>
struct direct_bases;

template<class Face>
using direct_bases_t = typename direct_bases<Face>::type;

namespace detail {
template<class Bases>
struct base_types;

template<class... Bases>
struct base_types<type_list<Bases...>> {
    using type = type_list<typename Bases::type...>;
};

template<class Seen, class Pending>
struct collect_interfaces;

template<class Seen>
struct collect_interfaces<Seen, type_list<>> {
    using type = Seen;
};

template<class... Seen, class Next, class... Rest>
struct collect_interfaces<
    type_list<Seen...>,
    type_list<Next, Rest...>>
    : collect_interfaces<
          type_list<Seen..., Next>,
          concat_t<typename base_types<direct_bases_t<Next>>::type, type_list<Rest...>>> {};

template<class... Seen, class Next, class... Rest>
    requires(std::same_as<Next, Seen> || ...)
struct collect_interfaces<
    type_list<Seen...>,
    type_list<Next, Rest...>>
    : collect_interfaces<
          type_list<Seen...>,
          type_list<Rest...>> {};

template<class... Faces>
using interface_closure_t = typename collect_interfaces<type_list<>, type_list<Faces...>>::type;
}
}

// ============================================================================
// Interface id
// ============================================================================

namespace fct {
using interface_id_t = char const *;
using interface_version_t = std::uint64_t;

using type_info_version_t = std::uint64_t;
using type_info_query_fn_t = void const *(*)(void *, interface_id_t);

struct type_info {
    type_info_version_t version;
    type_info_query_fn_t query;
};

template<class Face>
struct interface_id;

template<class Face>
concept has_interface_id = requires {
    { interface_id<Face>::value } -> std::convertible_to<interface_id_t>;
};

template<has_interface_id Face>
inline constexpr interface_id_t interface_id_v = interface_id<Face>::value;

inline bool interface_id_equal(interface_id_t lhs, interface_id_t rhs) noexcept {
    if (lhs == rhs) {
        return true;
    }
    std::string_view lsv(lhs);
    std::string_view rsv(rhs);
    return !lsv.empty() && !rsv.empty() && lsv == rsv;
}
}

// ============================================================================
// Vtable casts
// ============================================================================

namespace fct {
template<class To, class From>
struct vtable_upcast_customizer;

template<class To, class From>
concept upcastable_from = requires(vtable_t<From> const *vt) {
    { vtable_upcast_customizer<To, From>()(vt) } -> std::same_as<vtable_t<To> const *>;
};

template<class To, class From>
    requires upcastable_from<To, From>
constexpr vtable_t<To> const *vtable_upcast_from(vtable_t<From> const *vt) noexcept {
    if (vt == nullptr) {
        return nullptr;
    }
    return vtable_upcast_customizer<To, From>()(vt);
}

template<class To, class... From>
concept upcastable_from_any = (upcastable_from<To, From> || ...);

template<class To, class Source, class... Bases>
    requires upcastable_from_any<To, typename Bases::type...>
constexpr vtable_t<To> const *vtable_upcast_from_any(Source &&source, type_list<Bases...>) noexcept {
    constexpr std::array matches = {upcastable_from<To, typename Bases::type>...};
    constexpr auto index = std::ranges::find(matches, true) - matches.begin();
    using Base = typename type_list<Bases...>::template at<index>;
    return vtable_upcast_from<To, typename Base::type>(std::invoke(Base::converter, std::forward<Source>(source)));
}

template<class To>
struct vtable_upcast_customizer<To, To> {
    constexpr vtable_t<To> const *operator()(vtable_t<To> const *vt) const noexcept {
        return vt;
    }
};

template<class To, class From>
struct vtable_upcast_customizer {
    constexpr auto operator()(vtable_t<From> const *vt) const noexcept
        requires requires { vtable_upcast_from_any<To>(vt, direct_bases_t<From>{}); }
    {
        return vtable_upcast_from_any<To>(vt, direct_bases_t<From>{});
    }
};

template<class To>
struct vtable_query_customizer;

template<class To>
concept queryable = requires(type_info const *rtti, void *data) {
    { vtable_query_customizer<To>()(rtti, data) } -> std::same_as<vtable_t<To> const *>;
};

template<queryable To>
constexpr vtable_t<To> const *vtable_query_with(type_info const *rtti, void *data) noexcept {
    if (data == nullptr) {
        return nullptr;
    }
    return vtable_query_customizer<To>()(rtti, data);
}

template<has_interface_id To>
struct vtable_query_customizer<To> {
    constexpr vtable_t<To> const *operator()(type_info const *rtti, void *data) const noexcept {
        return static_cast<vtable_t<To> const *>(rtti->query(data, interface_id_v<To>));
    }
};
}

// ============================================================================
// Facade
//
// Each facade inherits one Name::bind<Slot> per distinct name type.
// The bound slot dispatches the overload set through the facade's handle.
// ============================================================================

namespace fct {
namespace detail {
template<auto Invoker>
struct callable : decltype(Invoker) {
    constexpr callable() : decltype(Invoker)(Invoker) {}

    using decltype(Invoker)::operator();
};

template<class Methods>
struct overload;

template<class... Methods>
struct overload<type_list<Methods...>> : callable<Methods::invoker>... {
    using callable<Methods::invoker>::operator()...;
};

template<class Seen, class Pending>
struct unique_names;

template<class Seen>
struct unique_names<Seen, type_list<>> {
    using type = Seen;
};

template<class... Seen, class Name, class... Rest>
struct unique_names<
    type_list<Seen...>,
    type_list<Name, Rest...>>
    : unique_names<
          type_list<Seen..., Name>,
          type_list<Rest...>> {};

template<class... Seen, class Name, class... Rest>
    requires(std::same_as<Name, Seen> || ...)
struct unique_names<
    type_list<Seen...>,
    type_list<Name, Rest...>>
    : unique_names<
          type_list<Seen...>,
          type_list<Rest...>> {};

template<class Faces>
struct methods_of;

template<class... Faces>
struct methods_of<type_list<Faces...>> : concat<direct_methods_t<Faces>...> {};

template<class Self, class Faces, class Methods = typename methods_of<Faces>::type>
struct facade_for;

template<class Self, class Faces, class... Methods>
struct facade_for<Self, Faces, type_list<Methods...>> {
    template<class Name>
    struct slot {
        using methods = concat_t<std::conditional_t<
            std::same_as<Name, typename Methods::name>,
            type_list<Methods>,
            type_list<>>...>;

        Self const *self;

        template<class Forwarder>
        explicit slot(Forwarder const *name)
            : self(reinterpret_cast<Self const *>(static_cast<type const *>(name))) {}

        template<class... Args>
            requires std::invocable<overload<methods>, Self const &, Args...>
        decltype(auto) operator()(Args &&...args) const {
            return overload<methods>{}(*self, std::forward<Args>(args)...);
        }
    };

    template<class Names>
    struct compose;

    template<class... Names>
    struct compose<type_list<Names...>> {
        using type = struct : Names::template bind<slot<Names>>... {};
    };

    using type = compose<typename unique_names<type_list<>, type_list<typename Methods::name...>>::type>::type;
};
}

template<class Self, class Face>
using facade = typename detail::facade_for<Self, type_list<Face>>::type;

template<class Self, class... Faces>
using combined_facade = typename detail::facade_for<Self, detail::interface_closure_t<Faces...>>::type;
}

// ============================================================================
// Bare handle
// ============================================================================

namespace fct {
template<class Impl, class... Faces>
struct typed_facets;

template<class... Faces>
using facets = typed_facets<void, Faces...>;

using opaque = facets<>;

template<class Impl, class... Faces>
struct typed_facets {
    using data_type = Impl;
    using vtable_tuple = fct::tuple<vtable_t<Faces> const *...>;

    constexpr typed_facets() noexcept = default;

    constexpr explicit typed_facets(Impl *d, type_info const *r, vtable_t<Faces> const *...vs) noexcept : data(d), rtti(r), vtables{vs...} {}

    // ---- castability ------------------------------------------------------

    template<class To>
    static constexpr bool is_upcastable = upcastable_from_any<To, Faces...>;

    template<class To>
    static constexpr bool is_queryable = queryable<To>;

    template<class To>
    static constexpr bool is_upcastable_or_queryable = is_upcastable<To> || is_queryable<To>;

    template<class Base>
    static constexpr bool is_implicit_convertible = std::is_same_v<Base, Impl> || std::is_same_v<Base, void>;

    template<class Base>
    static constexpr bool is_convertible = is_implicit_convertible<Base> || std::is_same_v<Impl, void>;

    // ---- vtable casts -----------------------------------------------------

    template<class To>
        requires(is_upcastable<To>)
    constexpr vtable_t<To> const *vtable_upcast() const noexcept {
        return [this]<std::size_t... I>(std::index_sequence<I...>) {
            return vtable_upcast_from_any<To>(*this, type_list<direct_base<Faces, &typed_facets::template vtable_at<I>>...>{});
        }(std::index_sequence_for<Faces...>{});
    }

    template<class To>
        requires(is_queryable<To>)
    constexpr vtable_t<To> const *vtable_query() const noexcept {
        return vtable_query_with<To>(rtti, data);
    }

    template<class To>
        requires(is_upcastable_or_queryable<To>)
    constexpr vtable_t<To> const *vtable_upcast_or_query() const noexcept {
        if constexpr (is_upcastable<To>) {
            return vtable_upcast<To>();
        } else {
            return vtable_query<To>();
        }
    }

    // ---- conversions ------------------------------------------------------

    template<class Base, class... Tos>
        requires(is_convertible<Base> && (is_upcastable_or_queryable<Tos> && ...) && !std::is_same_v<typed_facets<Base, Tos...>, typed_facets>)
    explicit(!(is_implicit_convertible<Base> && (is_upcastable<Tos> && ...)))
    operator typed_facets<Base, Tos...>() const noexcept {
        return typed_facets<Base, Tos...>(static_cast<Base *>(data), rtti, vtable_upcast_or_query<Tos>()...);
    }

    // ---- accessors ---------------------------------------------------------

    auto *get() const noexcept {
        return data;
    }

    template<std::size_t K>
    constexpr auto const *vtable_at() const noexcept {
        return fct::get<K>(vtables);
    }

    explicit operator bool() const noexcept {
        return data != nullptr &&
               fct::apply(
                   [](vtable_t<Faces> const *...vs) {
                       return ((vs != nullptr) && ...);
                   },
                   vtables);
    }

    auto &operator*() const noexcept {
        return reinterpret_cast<combined_facade<typed_facets, Faces...> const &>(*this);
    }

    auto *operator->() const noexcept {
        return reinterpret_cast<combined_facade<typed_facets, Faces...> const *>(this);
    }

    template<class Face>
    auto *as() const noexcept {
        return reinterpret_cast<facade<typed_facets, Face> const *>(this);
    }

private:
    Impl *data = nullptr;
    type_info const *rtti = nullptr;
    vtable_tuple vtables = {};
};
}

// ============================================================================
// Interface for querying vtables and type information
// ============================================================================

namespace fct {
template<class Impl, class... Faces>
struct type_info_for;

template<class Impl, class... Faces>
concept has_type_info_for = requires {
    { type_info_for<Impl, Faces...>::value } -> std::convertible_to<type_info const *>;
};

template<class Impl, class... Faces>
    requires has_type_info_for<Impl, Faces...>
inline constexpr type_info const *type_info_for_v = type_info_for<Impl, Faces...>::value;

template<class Impl, class Face>
struct vtable_for;

template<class Impl, class Face>
concept has_vtable_for = requires {
    { vtable_for<Impl, Face>::value } -> std::convertible_to<vtable_t<Face> const *>;
};

template<class Impl, class Face>
    requires has_vtable_for<Impl, Face>
inline constexpr vtable_t<Face> const *vtable_for_v = vtable_for<Impl, Face>::value;

template<class... Faces, class Impl>
auto as_facets(Impl *data) {
    return typed_facets<Impl, Faces...>(data, type_info_for_v<Impl, Faces...>, vtable_for_v<Impl, Faces>...);
}
}

// ============================================================================
// Runtime interface query
// ============================================================================

namespace fct {
template<class Impl, class... Faces>
    requires(has_vtable_for<Impl, Faces> && ...)
struct type_info_for<Impl, Faces...> {
private:
    template<class Face, class Closure = detail::interface_closure_t<Face>>
    struct query_face;

    template<class Face, class... Tos>
    struct query_face<Face, type_list<Tos...>> {
        template<class To>
        static bool query_interface(interface_id_t id, void const *&result) {
            if constexpr (has_interface_id<To>) {
                if (interface_id_equal(id, interface_id_v<To>)) {
                    result = vtable_upcast_from<To, Face>(vtable_for_v<Impl, Face>);
                    return true;
                }
            }
            return false;
        }

        static bool query(interface_id_t id, void const *&result) {
            return (query_interface<Tos>(id, result) || ...);
        }
    };

public:
    static constexpr type_info content = {
        .version = FACET_TYPE_INFO_VERSION,
        .query = [](void *, interface_id_t id) -> void const * {
            void const *result = nullptr;
            (query_face<Faces>::query(id, result) || ...);
            return result;
        },
    };

    static constexpr type_info const *value = &content;
};
}
