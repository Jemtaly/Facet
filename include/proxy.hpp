#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <meta>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#define PROXY_VERSION 0

namespace proxy {
namespace detail {
// Intentionally non-constexpr and undefined: reaching a call to it during
// constant evaluation produces a compile-time error that shows `msg`.
void compile_time_error(char const *msg);
}
}

// ============================================================================
// Reflection utilities
// ============================================================================

namespace proxy {
namespace detail {
namespace meta = std::meta;

template<class... Is>
constexpr inline meta::info interface_list_v[] = {^^Is...};

consteval meta::access_context all_ctx() {
    return meta::access_context::unchecked();
}

consteval meta::access_context pub_ctx() {
    return meta::access_context::unprivileged();
}

template<class Cls>
constexpr inline std::span<meta::info const> all_data_members_v = std::define_static_array(
    meta::nonstatic_data_members_of(meta::dealias(^^Cls), all_ctx()));
}
}

// ============================================================================
// Interface protocol
//
// These templates are the non-reflective protocol consumed by the rest of the
// library. Their defaults are generated from an interface specification, but
// users may specialize them for interfaces that cannot or should not use the
// automatic reflection path. A custom vtable starts with `self` and provides
// one base slot per direct_bases entry; each base converter extracts that slot.
// ============================================================================

namespace proxy {
struct fixed_string {
    static constexpr std::size_t capacity = 64;

    char data[capacity] = {};
    std::size_t size = 0;

    constexpr std::string_view view() const noexcept {
        return {data, size};
    }
};

consteval fixed_string make_fixed_string(std::string_view s) {
    if (s.size() >= fixed_string::capacity) {
        detail::compile_time_error("proxy: annotation string too long (max 63 chars)");
    }
    fixed_string result;
    for (std::size_t i = 0; i < s.size(); ++i) {
        result.data[i] = s[i];
    }
    result.size = s.size();
    return result;
}

struct direct_method_t {
    fixed_string name;
    std::meta::info invoker;
};

template<class Invoker>
consteval direct_method_t direct_method(std::string_view name, Invoker invoker) {
    return {make_fixed_string(name), std::meta::reflect_constant(invoker)};
}

struct direct_base_t {
    std::meta::info type;
    std::meta::info upcast;
};

template<class Base, class Upcast>
consteval direct_base_t direct_base(Upcast upcast) {
    return {^^Base, std::meta::reflect_constant(upcast)};
}

template<class I>
struct direct_methods;

template<class I>
constexpr inline std::span<direct_method_t const> direct_methods_v = direct_methods<I>::value;

template<class I>
struct direct_bases;

template<class I>
constexpr inline std::span<direct_base_t const> direct_bases_v = direct_bases<I>::value;

namespace detail {
// Appends I and all of its transitive bases (depth-first, without duplicates).
// All base traversal goes through direct_bases, including the automatic path.
template<class I>
consteval void append_interface_closure(std::vector<meta::info> &out) {
    for (meta::info existing : out) {
        if (meta::is_same_type(existing, ^^I)) {
            return;
        }
    }
    out.push_back(^^I);
    template for (constexpr direct_base_t base : direct_bases_v<I>) {
        using Base = typename[:base.type:];
        append_interface_closure<Base>(out);
    }
}

template<class I>
constexpr inline std::span<meta::info const> interface_closure_v = [] {
    std::vector<meta::info> out;
    append_interface_closure<I>(out);
    return std::define_static_array(out);
}();

template<class... Is>
constexpr inline std::span<meta::info const> combined_interface_closure_v = [] {
    std::vector<meta::info> out;
    template for (constexpr meta::info i : interface_list_v<Is...>) {
        using I = typename[:i:];
        append_interface_closure<I>(out);
    }
    return std::define_static_array(out);
}();
}
}

// ============================================================================
// Interface id
// ============================================================================

namespace proxy {
using interface_id_t = char const *;

template<class I>
struct interface_id {
    static constexpr char value[] = "";
};

template<class I>
constexpr inline interface_id_t interface_id_v = interface_id<I>::value;

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

namespace proxy {
using type_info_version_t = std::uint64_t;
using query_vtable_ptr_fn = void const *(*)(void *, interface_id_t);

struct type_info {
    type_info_version_t version;
    query_vtable_ptr_fn query_vtable_ptr;
};

template<class I>
struct vtable;

template<class I>
using vtable_t = typename vtable<I>::type;

template<class To, class From>
struct vtable_upcast_customizer;

template<class To>
struct vtable_dycast_customizer;

template<class To, class From>
concept upcastable_from = vtable_upcast_customizer<To, From>::value;

template<class To>
concept dycastable = vtable_dycast_customizer<To>::value;

template<class To, class From>
constexpr vtable_t<To> const *vtable_upcast(vtable_t<From> const *vt) noexcept {
    if (vt == nullptr) {
        return nullptr;
    }
    return vtable_upcast_customizer<To, From>()(vt);
}

template<class To>
constexpr vtable_t<To> const *vtable_dycast(type_info const *rtti, void *data) noexcept {
    if (data == nullptr) {
        return nullptr;
    }
    return vtable_dycast_customizer<To>()(rtti, data);
}

template<class To, class From>
struct vtable_upcast_customizer {
    static constexpr bool value = [] {
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
    }();

    constexpr vtable_t<To> const *operator()(vtable_t<From> const *vt) const noexcept {
        if constexpr (std::is_same_v<To, From>) {
            return vt;
        }
        template for (constexpr direct_base_t base : direct_bases_v<From>) {
            using Base = typename[:base.type:];
            if constexpr (upcastable_from<To, Base>) {
                return vtable_upcast<To, Base>([:base.upcast:](vt));
            }
        }
        __builtin_unreachable();
    }
};

template<class To>
struct vtable_dycast_customizer {
    static constexpr bool value = true;

    constexpr vtable_t<To> const *operator()(type_info const *rtti, void *data) const noexcept {
        return static_cast<vtable_t<To> const *>(rtti->query_vtable_ptr(data, interface_id_v<To>));
    }
};
}

// ============================================================================
// Facade
//
// facade_t<Self, Is...> is an aggregate with one slot per method name. Each
// slot combines the invokers for that name and dispatches through its handle.
// ============================================================================

namespace proxy {
template<class Impl, class... Is>
struct object_handle;

template<class... Is>
using interface_handle = object_handle<void, Is...>;

using data_handle = interface_handle<>;

namespace detail {
template<class Self, class I>
struct facade_def;

template<class Self, class... Is>
struct combined_facade_def;
}

template<class Self, class... Is>
using facade_t = typename detail::facade_def<Self, Is...>::type;

template<class Self, class... Is>
using combined_facade_t = typename detail::combined_facade_def<Self, Is...>::type;

namespace detail {
template<class... Invokers>
struct overload : Invokers... {
    using Invokers::invoke...;
};

template<class Self, class Facade, std::size_t GroupIndex, class... Invokers>
struct slot {
    template<class... Args>
    decltype(auto) operator()(Args &&...args) const {
        constexpr auto offset = meta::offset_of(all_data_members_v<Facade>[GroupIndex]).bytes;
        auto const *facade = reinterpret_cast<Facade const *>(reinterpret_cast<std::byte const *>(this) - offset);
        auto self = *reinterpret_cast<Self const *>(facade);
        return overload<Invokers...>::invoke(self, std::forward<Args>(args)...);
    }
};

template<class I, auto Callable>
struct direct_invoker {
    template<class... Args>
        requires requires(interface_handle<I> handle, Args &&...args) {
            Callable(handle, std::forward<Args>(args)...);
        }
    static decltype(auto) invoke(interface_handle<I> handle, Args &&...args) {
        return Callable(handle, std::forward<Args>(args)...);
    }
};

struct dispatch_group {
    std::string name;
    std::vector<meta::info> entries;
};

consteval dispatch_group &find_or_create_group(std::vector<dispatch_group> &table, std::string_view name) {
    for (dispatch_group &group : table) {
        if (group.name == name) {
            return group;
        }
    }
    return table.emplace_back(std::string(name));
}

template<class I>
consteval void append_dispatch_table(std::vector<dispatch_group> &table) {
    for (direct_method_t method : direct_methods_v<I>) {
        find_or_create_group(table, method.name.view()).entries.push_back(meta::dealias(meta::substitute(^^direct_invoker, {^^I, method.invoker})));
    }
}

consteval void define_facade_members(meta::info type, meta::info self, std::span<dispatch_group const> table) {
    std::vector<meta::info> members;
    for (auto const &[name, entries] : table) {
        std::vector<meta::info> slot_args = {self, type, meta::reflect_constant(members.size())};
        for (auto entry : entries) {
            slot_args.push_back(entry);
        }
        members.push_back(meta::data_member_spec(meta::substitute(^^slot, slot_args), {.name = name, .no_unique_address = true}));
    }
    meta::define_aggregate(type, members);
}

template<class Self, class I>
struct facade_def {
    struct type;

    consteval {
        std::vector<dispatch_group> table;
        append_dispatch_table<I>(table);
        define_facade_members(^^type, ^^Self, table);
    }
};

template<class Self, class... Is>
struct combined_facade_def {
    struct type;

    consteval {
        std::vector<dispatch_group> table;
        template for (constexpr meta::info i : combined_interface_closure_v<Is...>) {
            using I = typename[:i:];
            append_dispatch_table<I>(table);
        }
        define_facade_members(^^type, ^^Self, table);
    }
};
}
}

// ============================================================================
// Bare handle
// ============================================================================

namespace proxy {
template<class Data, class... Is>
struct object_handle {
    using data_type = Data;
    using vtable_tuple = std::tuple<vtable_t<Is> const *...>;

    Data *data = nullptr;
    type_info const *rtti = nullptr;
    vtable_tuple vtables = {};

    constexpr object_handle() noexcept = default;

    constexpr object_handle(Data *d, type_info const *r, vtable_tuple t) noexcept : data(d), rtti(r), vtables(t) {}

    constexpr explicit object_handle(Data *d, type_info const *r, vtable_t<Is> const *...vs) noexcept : data(d), rtti(r), vtables(vs...) {}

    // ---- castability ------------------------------------------------------

    template<class To>
    static constexpr bool can_upcast_to = (upcastable_from<To, Is> || ...);

    template<class To>
    static constexpr bool can_dycast_to = dycastable<To>;

    template<class To>
    static constexpr bool can_cast_to = can_upcast_to<To> || can_dycast_to<To>;

    template<class Base, class... Js>
    static constexpr bool can_convert_to =
        (std::is_same_v<Data, Base> || std::is_same_v<Base, void>) &&
        (can_cast_to<Js> && ...);

    template<class... Js>
    static constexpr bool can_implicitly_convert_to = (can_upcast_to<Js> && ...);

    // ---- vtable casts -----------------------------------------------------

    template<class To>
        requires(can_upcast_to<To>)
    constexpr vtable_t<To> const *upcast_vtable() const noexcept {
        template for (constexpr std::size_t K : std::views::iota(0uz, sizeof...(Is))) {
            using I = Is...[K];
            if constexpr (upcastable_from<To, I>) {
                return vtable_upcast<To, I>(std::get<K>(vtables));
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

    template<class Base, class... Js>
        requires(can_convert_to<Base, Js...> && !std::is_same_v<object_handle<Base, Js...>, object_handle>)
    explicit(!can_implicitly_convert_to<Js...>)
    operator object_handle<Base, Js...>() const noexcept {
        return object_handle<Base, Js...>(data, rtti, cast_vtable<Js>()...);
    }

    explicit operator bool() const noexcept {
        return data != nullptr &&
               std::apply(
                   [](vtable_t<Is> const *...vs) {
                       return ((vs != nullptr) && ...);
                   },
                   vtables);
    }

    auto &operator*() const noexcept {
        return reinterpret_cast<combined_facade_t<object_handle, Is...> const &>(*this);
    }

    auto *operator->() const noexcept {
        return reinterpret_cast<combined_facade_t<object_handle, Is...> const *>(this);
    }

    template<class I>
    auto *as() const noexcept {
        return reinterpret_cast<facade_t<object_handle, I> const *>(this);
    }

    auto *get() const noexcept {
        return data;
    }
};
}

// ============================================================================
// Implementation lookup and thunks
//
// For each interface method, public members of Data and its public bases are
// searched by identifier. Exact matches are preferred in this order: static
// functions taking object_handle<Data, I>, then member functions. If neither
// has an exact match, the same order is used for invocable compatible matches.
// ============================================================================

namespace proxy {
template<class Data, class I>
struct vtable_implements;

template<class Data, class I>
concept implements = requires { vtable_implements<Data, I>::value; };

template<class Data, class I>
constexpr vtable_t<I> const *vtable_implements_v = vtable_implements<Data, I>::value;

template<class Data, class... Is>
struct static_policy {
    template<class I>
    static constexpr vtable_t<I> const *vtable_ptr = vtable_implements_v<Data, I>;

    static constexpr type_info rtti = {
        .version = PROXY_VERSION,
        .query_vtable_ptr = [](void *, interface_id_t id) -> void const * {
            template for (constexpr std::meta::info i : detail::interface_list_v<Is...>) {
                using I = typename[:i:];
                template for (constexpr std::meta::info j : detail::interface_closure_v<I>) {
                    using J = typename[:j:];
                    if (interface_id_equal(id, interface_id_v<J>)) {
                        return vtable_upcast<J, I>(vtable_ptr<I>);
                    }
                }
            }
            return nullptr;
        },
    };

    static object_handle<Data, Is...> make_handle(Data *data) noexcept {
        return object_handle<Data, Is...>(data, &rtti, vtable_ptr<Is>...);
    }
};

template<class... Is, class Impl>
auto as_handle(Impl *data) {
    return static_policy<Impl, Is...>::make_handle(data);
}
}
