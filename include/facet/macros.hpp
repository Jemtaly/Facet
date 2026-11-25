#pragma once

#include <facet.hpp>

namespace fct {
namespace detail {
template<interface_version_t Version, auto Fallback>
struct method_since {
    static constexpr auto version = Version;
    static constexpr auto fallback = Fallback;
};

template<class Name, class Signature, auto Dispatch = Name::dispatcher, class Since = void>
struct member_method {
    template<auto Dispatcher>
    using dispatch_via = member_method<Name, Signature, Dispatcher, Since>;

    template<interface_version_t Version, auto Fallback>
    using since = member_method<Name, Signature, Dispatch, method_since<Version, Fallback>>;
};

template<class Face, class Entries, class Indices = std::make_index_sequence<Entries::size>>
struct interface_layout;

template<class Face, class... Entries, std::size_t... Indices>
struct interface_layout<Face, type_list<Entries...>, std::index_sequence<Indices...>> {
    template<class Base>
    struct entry_traits {
        using pointer = vtable_t<Base> const *;

        template<auto Converter>
        using bases = type_list<direct_base<Base, Converter>>;
        template<auto>
        using methods = type_list<>;
    };

    template<class Name, class R, class... Args, auto Dispatch, class Since>
    struct entry_traits<member_method<Name, R(Args...), Dispatch, Since>> {
        using pointer = R (*)(facets<Face>, Args...);

        template<auto>
        using bases = type_list<>;
        template<auto Invoker>
        using methods = type_list<direct_method<Name, Invoker>>;
    };

    template<class Entry>
    using pointer_t = typename entry_traits<Entry>::pointer;

    struct table {
        interface_version_t version;
        [[no_unique_address]] fct::tuple<pointer_t<Entries>...> entries;
    };

    template<std::size_t I, class Base>
    struct entry_at {
        constexpr pointer_t<Base> operator()(table const *vt) const noexcept {
            return fct::get<I>(vt->entries);
        }
    };

    template<std::size_t I, class Name, class R, class... Args, auto Dispatch, class Since>
    struct entry_at<I, member_method<Name, R(Args...), Dispatch, Since>> {
        R operator()(facets<Face> self, Args... args) const {
            auto const *table = self.template vtable_at<0>();
            if constexpr (!std::is_void_v<Since>) {
                if (table->version < Since::version) {
                    return Since::fallback(self, std::forward<Args>(args)...);
                }
            }
            return fct::get<I>(table->entries)(self, std::forward<Args>(args)...);
        }
    };

    template<std::size_t I, class Entry>
    static constexpr entry_at<I, Entry> entry_at_v = {};

    using bases = concat_t<typename entry_traits<Entries>::template bases<entry_at_v<Indices, Entries>>...>;
    using methods = concat_t<typename entry_traits<Entries>::template methods<entry_at_v<Indices, Entries>>...>;

    template<class Impl, class Entry>
    struct pointer_for;

    template<class Impl, class Base>
        requires has_vtable_for<Impl, Base>
    struct pointer_for<Impl, Base> {
        static constexpr auto value = vtable_for_v<Impl, Base>;
    };

    template<class Impl, class Name, class R, class... Args, auto Dispatch, class Since>
        requires requires(typed_facets<Impl, Face> typed_self, Args... args) {
            { Dispatch(typed_self, std::forward<Args>(args)...) } -> std::convertible_to<R>;
        }
    struct pointer_for<Impl, member_method<Name, R(Args...), Dispatch, Since>> {
        static R call(facets<Face> self, Args... args) {
            typed_facets<Impl, Face> typed_self(self);
            return Dispatch(typed_self, std::forward<Args>(args)...);
        }

        static constexpr auto value = &call;
    };

    template<class Impl>
        requires((requires { pointer_for<Impl, Entries>::value; }) && ...)
    static constexpr table table_for = {Face::version, {pointer_for<Impl, Entries>::value...}};
};
}

template<class... Entries>
struct interface {
    static constexpr interface_version_t version = 0;

    template<class Face>
    using facet_layout = detail::interface_layout<Face, type_list<Entries...>>;
};

namespace detail {
template<class Face>
using declared_layout_t = typename Face::template facet_layout<Face>;

template<class Face, class = void>
struct interface_vtable {};

template<class Face>
struct interface_vtable<Face, std::void_t<declared_layout_t<Face>>> {
    using type = typename declared_layout_t<Face>::table;
};

template<class Face>
concept declared_interface = std::same_as<vtable_t<Face>, typename declared_layout_t<Face>::table>;
}

// An explicit or reflected vtable specialization takes precedence over an
// interface declaration inherited from a base.
template<class Face>
struct vtable : detail::interface_vtable<Face> {};

template<detail::declared_interface Face>
struct direct_bases<Face> {
    using type = typename detail::declared_layout_t<Face>::bases;
};

template<detail::declared_interface Face>
struct direct_methods<Face> {
    using type = typename detail::declared_layout_t<Face>::methods;
};

template<class Impl, detail::declared_interface Face>
    requires requires { detail::declared_layout_t<Face>::template table_for<Impl>; }
struct vtable_for<Impl, Face> {
    static constexpr auto value = &detail::declared_layout_t<Face>::template table_for<Impl>;
};
}

#define FACET_METHOD(type_name, member_name)                                                \
    struct type_name##_facet_name {                                                         \
        template<class FacetSlot>                                                           \
        struct bind_def {                                                                   \
            using type = struct {                                                           \
                template<class... FacetArgs>                                                \
                    requires ::std::invocable<FacetSlot, FacetArgs...>                      \
                decltype(auto) member_name(FacetArgs &&...args) const {                     \
                    return FacetSlot(this)(::std::forward<FacetArgs>(args)...);             \
                }                                                                           \
            };                                                                              \
        };                                                                                  \
                                                                                            \
        template<class FacetSlot>                                                           \
        using bind = bind_def<FacetSlot>::type;                                             \
                                                                                            \
        static constexpr auto dispatcher = [](auto const &self, auto &&...args)             \
            -> decltype(self.get()->member_name(::std::forward<decltype(args)>(args)...)) { \
            return self.get()->member_name(::std::forward<decltype(args)>(args)...);        \
        };                                                                                  \
    };                                                                                      \
                                                                                            \
    template<class FacetSignature>                                                          \
    using type_name = ::fct::detail::member_method<type_name##_facet_name, FacetSignature>
