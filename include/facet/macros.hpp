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
    using face_type = Face;

    template<class Base>
    struct pointer {
        using type = vtable_t<Base> const *;
    };

    template<class Name, class R, class... Args, auto Dispatch, class Since>
    struct pointer<member_method<Name, R(Args...), Dispatch, Since>> {
        using type = R (*)(facets<Face>, Args...);
    };

    template<class Entry>
    using pointer_t = typename pointer<Entry>::type;

    struct vtable_type {
        interface_version_t version;
        [[no_unique_address]] fct::tuple<pointer_t<Entries>...> entries;
    };

    template<std::size_t I, class Base>
    struct entry_traits {
        struct entry_type {
            constexpr pointer_t<Base> operator()(vtable_type const *vt) const noexcept {
                return fct::get<I>(vt->entries);
            }
        };

        static constexpr entry_type entry = {};

        using bases = type_list<direct_base<Base, entry>>;
        using methods = type_list<>;
    };

    template<std::size_t I, class Name, class R, class... Args, auto Dispatch, class Since>
    struct entry_traits<I, member_method<Name, R(Args...), Dispatch, Since>> {
        struct entry_type {
            R operator()(facets<Face> self, Args... args) const {
                auto const *vt = self.template vtable_at<0>();
                if constexpr (!std::is_void_v<Since>) {
                    if (vt->version < Since::version) {
                        return Since::fallback(self, std::forward<Args>(args)...);
                    }
                }
                return fct::get<I>(vt->entries)(self, std::forward<Args>(args)...);
            }
        };

        static constexpr entry_type entry = {};

        using bases = type_list<>;
        using methods = type_list<direct_method<Name, entry>>;
    };

    template<std::size_t I, class Entry>
    using bases_of = typename entry_traits<I, Entry>::bases;

    template<std::size_t I, class Entry>
    using methods_of = typename entry_traits<I, Entry>::methods;

    using bases = concat_t<bases_of<Indices, Entries>...>;
    using methods = concat_t<methods_of<Indices, Entries>...>;

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
    static constexpr vtable_type vtable_for = {Face::version, {pointer_for<Impl, Entries>::value...}};
};
}

template<class Self, class... Entries>
struct interface {
    static constexpr interface_version_t version = 0;

    using layout = detail::interface_layout<Self, type_list<Entries...>>;
};

namespace detail {
template<class Face>
concept macro_interface = std::same_as<Face, typename Face::layout::face_type>;
}

template<detail::macro_interface Face>
struct vtable<Face> {
    using type = typename Face::layout::vtable_type;
};

template<detail::macro_interface Face>
struct direct_bases<Face> {
    using type = typename Face::layout::bases;
};

template<detail::macro_interface Face>
struct direct_methods<Face> {
    using type = typename Face::layout::methods;
};

template<class Impl, detail::macro_interface Face>
    requires requires { Face::layout::template vtable_for<Impl>; }
struct vtable_for<Impl, Face> {
    static constexpr auto value = &Face::layout::template vtable_for<Impl>;
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
