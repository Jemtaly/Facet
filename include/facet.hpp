#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <meta>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#define FACET_TYPE_INFO_VERSION 0

namespace fct {
namespace detail {
// Intentionally non-constexpr and undefined: reaching a call to it during
// constant evaluation produces a compile-time error that shows `msg`.
void compile_time_error(char const *msg);
}
}

// ============================================================================
// Reflection utilities
// ============================================================================

namespace fct {
namespace detail {
namespace meta = std::meta;

template<class... Ts>
constexpr inline meta::info type_list_v[] = {^^Ts...};

consteval meta::access_context all_ctx() {
    return meta::access_context::unchecked();
}

consteval meta::access_context pub_ctx() {
    return meta::access_context::unprivileged();
}

template<class T>
constexpr inline std::span<meta::info const> all_data_members_v =
    std::define_static_array(meta::nonstatic_data_members_of(^^T, all_ctx()));
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

struct static_string {
    char const *data;
    std::size_t size;

    template<char... Cs>
    static constexpr char data_of[] = {Cs..., '\0'};

    template<char... Cs>
    static constexpr size_t size_of = sizeof...(Cs);

    template<char... Cs>
    static constexpr static_string value_of = {data_of<Cs...>, size_of<Cs...>};

    static consteval static_string make(std::string_view sv) {
        std::vector<std::meta::info> cs;
        for (char c : sv) {
            cs.push_back(std::meta::reflect_constant(c));
        }
        return std::meta::extract<static_string>(std::meta::substitute(^^value_of, cs));
    }

    constexpr std::string_view view() const noexcept {
        return {data, size};
    }
};

struct direct_method_t {
    static_string name;
    std::meta::info invoker;
};

template<class Invoker>
consteval direct_method_t direct_method(std::string_view name, Invoker invoker) {
    return {static_string::make(name), std::meta::reflect_constant(invoker)};
}

struct direct_base_t {
    std::meta::info type;
    std::meta::info converter;
};

template<class Base, class Converter>
consteval direct_base_t direct_base(Converter converter) {
    return {^^Base, std::meta::reflect_constant(converter)};
}

template<class Face>
struct direct_methods;

template<class Face>
constexpr inline std::span<direct_method_t const> direct_methods_v = direct_methods<Face>::value;

template<class Face>
struct direct_bases;

template<class Face>
constexpr inline std::span<direct_base_t const> direct_bases_v = direct_bases<Face>::value;

namespace detail {
// Appends I and all of its transitive bases (depth-first, without duplicates).
// All base traversal goes through direct_bases, including the automatic path.
template<class Face>
consteval void append_interface_closure(std::vector<meta::info> &out) {
    for (meta::info face : out) {
        if (meta::is_same_type(face, ^^Face)) {
            return;
        }
    }
    out.push_back(^^Face);
    template for (constexpr direct_base_t base : direct_bases_v<Face>) {
        using Base = typename[:base.type:];
        append_interface_closure<Base>(out);
    }
}

template<class Face>
constexpr inline std::span<meta::info const> interface_closure_v = [] {
    std::vector<meta::info> out;
    append_interface_closure<Face>(out);
    return std::define_static_array(out);
}();

template<class... Faces>
constexpr inline std::span<meta::info const> combined_interface_closure_v = [] {
    std::vector<meta::info> out;
    template for (constexpr meta::info face : type_list_v<Faces...>) {
        using Face = typename[:face:];
        append_interface_closure<Face>(out);
    }
    return std::define_static_array(out);
}();
}
}

// ============================================================================
// Interface id
// ============================================================================

namespace fct {
using interface_id_t = char const *;

using type_info_version_t = std::uint64_t;
using query_vtable_func_t = void const *(*)(void *, interface_id_t);

struct type_info {
    type_info_version_t version;
    query_vtable_func_t query_vtable;
};

template<class Face>
struct interface_id;

template<class Face>
concept has_interface_id = requires { interface_id<Face>::value; };

template<class Face>
constexpr inline interface_id_t interface_id_v = interface_id<Face>::value;

inline bool interface_id_equal(interface_id_t lhs, interface_id_t rhs) noexcept {
    if (lhs == rhs) {
        return true;
    }
    std::string_view lsv(lhs);
    std::string_view rsv(rhs);
    return !lsv.empty() && !rsv.empty() && lsv == rsv;
}

// [[=local_id]] assigns a non-empty id used to find the interface during
// runtime cross-casts, but it is only valid within the current translation
// unit.
struct local_id_t {};

constexpr inline local_id_t local_id{};

// [[=uuid("...")]] assigns a non-empty id used to find the interface during
// runtime cross-casts.
struct uuid_t {
    static_string value;
};

consteval uuid_t uuid(std::string_view value) {
    return {static_string::make(value)};
}

namespace detail {
template<class Face>
concept has_local_id = !meta::annotations_of_with_type(^^Face, ^^local_id_t).empty();

template<class Face>
concept has_uuid = !meta::annotations_of_with_type(^^Face, ^^uuid_t).empty();

inline consteval interface_id_t get_interface_id(meta::info face) {
    auto annotations = meta::annotations_of_with_type(face, ^^uuid_t);
    return meta::extract<uuid_t>(annotations[0]).value.data;
}
}

template<detail::has_local_id Face>
struct interface_id<Face> {
    static constexpr char value[] = "";
};

template<detail::has_uuid Face>
struct interface_id<Face> {
    static constexpr interface_id_t value = detail::get_interface_id(^^Face);
};
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
constexpr vtable_t<To> const *vtable_upcast(vtable_t<From> const *vt) noexcept {
    if (vt == nullptr) {
        return nullptr;
    }
    return vtable_upcast_customizer<To, From>()(vt);
}

template<class To, class From>
    requires([] {
        if constexpr (std::is_same_v<To, From>) {
            return true;
        }
        template for (constexpr direct_base_t base : direct_bases_v<From>) {
            using Base = typename[:base.type:];
            if constexpr (upcastable_from<To, Base>) {
                return true;
            }
        }
        return false;
    }())
struct vtable_upcast_customizer<To, From> {
    constexpr vtable_t<To> const *operator()(vtable_t<From> const *vt) const noexcept {
        if constexpr (std::is_same_v<To, From>) {
            return vt;
        }
        template for (constexpr direct_base_t base : direct_bases_v<From>) {
            using Base = typename[:base.type:];
            if constexpr (upcastable_from<To, Base>) {
                return vtable_upcast<To, Base>([:base.converter:](vt));
            }
        }
        __builtin_unreachable();
    }
};

template<class To>
struct vtable_dycast_customizer;

template<class To>
concept dycastable = requires(type_info const *rtti, void *data) {
    { vtable_dycast_customizer<To>()(rtti, data) } -> std::same_as<vtable_t<To> const *>;
};

template<class To>
constexpr vtable_t<To> const *vtable_dycast(type_info const *rtti, void *data) noexcept {
    if (data == nullptr) {
        return nullptr;
    }
    return vtable_dycast_customizer<To>()(rtti, data);
}

template<has_interface_id To>
struct vtable_dycast_customizer<To> {
    constexpr vtable_t<To> const *operator()(type_info const *rtti, void *data) const noexcept {
        return static_cast<vtable_t<To> const *>(rtti->query_vtable(data, interface_id_v<To>));
    }
};
}

// ============================================================================
// Facade
//
// facade<Self, Faces...> is an aggregate with one slot per method name. Each
// slot combines the invokers for that name and dispatches through its handle.
// ============================================================================

namespace fct {
template<class Impl, class... Faces>
struct typed_facets;

template<class... Faces>
using facets = typed_facets<void, Faces...>;

using opaque = facets<>;

namespace detail {
template<class Self, class Face>
struct facade_def;

template<class Self, class... Faces>
struct combined_facade_def;
}

template<class Self, class... Faces>
using facade = typename detail::facade_def<Self, Faces...>::type;

template<class Self, class... Faces>
using combined_facade = typename detail::combined_facade_def<Self, Faces...>::type;

namespace detail {
template<auto Invoker>
struct invoker {
    template<class... Args>
        requires requires(Args &&...args) {
            Invoker(std::forward<Args>(args)...);
        }
    decltype(auto) operator()(Args &&...args) const {
        return Invoker(std::forward<Args>(args)...);
    }
};

template<auto... Invokers>
struct overload : invoker<Invokers>... {
    using invoker<Invokers>::operator()...;
};

template<class Facade, class Self, std::size_t GroupIndex, auto... Invokers>
struct slot {
    template<class... Args>
    decltype(auto) operator()(Args &&...args) const {
        constexpr auto offset = meta::offset_of(all_data_members_v<Facade>[GroupIndex]).bytes;
        auto const *facade = reinterpret_cast<Facade const *>(reinterpret_cast<std::byte const *>(this) - offset);
        auto self = *reinterpret_cast<Self const *>(facade);
        return overload<Invokers...>()(self, std::forward<Args>(args)...);
    }
};

struct dispatch_group {
    std::string name;
    std::vector<meta::info> invokers;
};

using dispatch_table = std::vector<dispatch_group>;

consteval dispatch_group &find_or_create_group(dispatch_table &table, std::string_view name) {
    for (dispatch_group &group : table) {
        if (group.name == name) {
            return group;
        }
    }
    return table.emplace_back(std::string(name));
}

template<class Face>
consteval void append_dispatch_table(dispatch_table &table) {
    for (direct_method_t method : direct_methods_v<Face>) {
        find_or_create_group(table, method.name.view()).invokers.push_back(method.invoker);
    }
}

consteval void define_facade_members(meta::info type, meta::info self, dispatch_table const &table) {
    std::vector<meta::info> members;
    for (auto const &[name, invokers] : table) {
        std::vector<meta::info> slot_args = {type, self, meta::reflect_constant(members.size())};
        for (auto invoker : invokers) {
            slot_args.push_back(invoker);
        }
        members.push_back(meta::data_member_spec(meta::substitute(^^slot, slot_args), {.name = name, .no_unique_address = true}));
    }
    meta::define_aggregate(type, members);
}

template<class Self, class Face>
struct facade_def {
    struct type;

    consteval {
        dispatch_table table;
        append_dispatch_table<Face>(table);
        define_facade_members(^^type, ^^Self, table);
    }
};

template<class Self, class... Faces>
struct combined_facade_def {
    struct type;

    consteval {
        dispatch_table table;
        template for (constexpr meta::info face : combined_interface_closure_v<Faces...>) {
            using Face = typename[:face:];
            append_dispatch_table<Face>(table);
        }
        define_facade_members(^^type, ^^Self, table);
    }
};
}
}

// ============================================================================
// Tuple
// ============================================================================

namespace fct {
namespace detail {
template<class... Ts>
struct tuple_def {
    struct type;

    consteval {
        meta::define_aggregate(^^type, {meta::data_member_spec(^^Ts, {.name = "_"})...});
    }
};
}

template<class... Ts>
using tuple = typename detail::tuple_def<Ts...>::type;

template<std::size_t I, class T>
constexpr decltype(auto) get(T &&t) noexcept {
    return (std::forward<T>(t).[:detail::all_data_members_v<std::remove_cvref_t<T>>[I]:]);
}

template<class F, class T>
constexpr decltype(auto) apply(F &&f, T &&t) {
    return [&]<std::size_t... Faces>(std::index_sequence<Faces...>) -> decltype(auto) {
        return std::invoke(std::forward<F>(f), get<Faces>(std::forward<T>(t))...);
    }(std::make_index_sequence<detail::all_data_members_v<std::remove_cvref_t<T>>.size()>{});
}
}

// ============================================================================
// Bare handle
// ============================================================================

namespace fct {
template<class Impl, class... Faces>
struct typed_facets {
    using data_type = Impl;
    using vtable_tuple = fct::tuple<vtable_t<Faces> const *...>;

    Impl *data = nullptr;
    type_info const *rtti = nullptr;
    vtable_tuple vtables = {};

    constexpr typed_facets() noexcept = default;

    constexpr typed_facets(Impl *d, type_info const *r, vtable_tuple t) noexcept : data(d), rtti(r), vtables(t) {}

    constexpr explicit typed_facets(Impl *d, type_info const *r, vtable_t<Faces> const *...vs) noexcept : data(d), rtti(r), vtables(vs...) {}

    // ---- castability ------------------------------------------------------

    template<class To>
    static constexpr bool can_upcast_to = (upcastable_from<To, Faces> || ...);

    template<class To>
    static constexpr bool can_dycast_to = dycastable<To>;

    template<class To>
    static constexpr bool can_cast_to = can_upcast_to<To> || can_dycast_to<To>;

    template<class Base, class... Tos>
    static constexpr bool can_convert_to =
        (std::is_same_v<Impl, Base> || std::is_same_v<Base, void>) &&
        (can_cast_to<Tos> && ...);

    template<class... Tos>
    static constexpr bool can_implicitly_convert_to = (can_upcast_to<Tos> && ...);

    // ---- vtable casts -----------------------------------------------------

    template<class To>
        requires(can_upcast_to<To>)
    constexpr vtable_t<To> const *upcast_vtable() const noexcept {
        template for (constexpr std::size_t K : std::views::iota(0uz, sizeof...(Faces))) {
            using Face = Faces...[K];
            if constexpr (upcastable_from<To, Face>) {
                return vtable_upcast<To, Face>(fct::get<K>(vtables));
            }
        }
    }

    template<class To>
        requires(can_dycast_to<To>)
    constexpr vtable_t<To> const *dycast_vtable() const noexcept {
        return vtable_dycast<To>(rtti, data);
    }

    template<class To>
        requires(can_cast_to<To>)
    constexpr vtable_t<To> const *cast_vtable() const noexcept {
        if constexpr (can_upcast_to<To>) {
            return upcast_vtable<To>();
        } else {
            return dycast_vtable<To>();
        }
    }

    // ---- conversions ------------------------------------------------------

    template<class Base, class... Tos>
        requires(can_convert_to<Base, Tos...> && !std::is_same_v<typed_facets<Base, Tos...>, typed_facets>)
    explicit(!can_implicitly_convert_to<Tos...>)
    operator typed_facets<Base, Tos...>() const noexcept {
        return typed_facets<Base, Tos...>(data, rtti, cast_vtable<Tos>()...);
    }

    // ---- utilities --------------------------------------------------------

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

    auto *get() const noexcept {
        return data;
    }
};
}

// ============================================================================
// Implementation lookup and thunks
//
// For each interface method, public members of Impl and its public bases are
// searched by identifier. Exact matches are preferred in this order: static
// functions taking typed_facets<Impl, I>, then member functions. If neither
// has an exact match, the same order is used for invocable compatible matches.
// ============================================================================

namespace fct {
template<class Impl, class Face>
struct vtable_implements;

template<class Impl, class Face>
concept implements = requires { vtable_implements<Impl, Face>::value; };

template<class Impl, class Face>
constexpr vtable_t<Face> const *vtable_implements_v = vtable_implements<Impl, Face>::value;

template<class Impl, class... Faces>
    requires(implements<Impl, Faces> && ...)
struct static_policy {
    template<class Face>
    static constexpr vtable_t<Face> const *vtable_ptr = vtable_implements_v<Impl, Face>;

    static constexpr type_info rtti = {
        .version = FACET_TYPE_INFO_VERSION,
        .query_vtable = [](void *, interface_id_t id) -> void const * {
            template for (constexpr std::meta::info face : detail::type_list_v<Faces...>) {
                using Face = typename[:face:];
                template for (constexpr std::meta::info to : detail::interface_closure_v<Face>) {
                    using To = typename[:to:];
                    if constexpr (has_interface_id<To>) {
                        if (interface_id_equal(id, interface_id_v<To>)) {
                            return vtable_upcast<To, Face>(vtable_ptr<Face>);
                        }
                    }
                }
            }
            return nullptr;
        },
    };

    static typed_facets<Impl, Faces...> make_facets(Impl *data) noexcept {
        return typed_facets<Impl, Faces...>(data, &rtti, vtable_ptr<Faces>...);
    }
};

template<class... Faces, class Impl>
auto as_facets(Impl *data) {
    return static_policy<Impl, Faces...>::make_facets(data);
}
}
