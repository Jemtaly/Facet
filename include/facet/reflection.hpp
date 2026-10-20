#pragma once

#include <cstddef>
#include <cstdint>
#include <meta>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <facet.hpp>

// ============================================================================
// Reflection utilities
// ============================================================================

namespace fct {
// [[=spec]] marks a type as a facet interface specification.
struct spec_t {};

constexpr inline spec_t spec{};

using interface_version_t = std::uint64_t;

// [[=version(n)]] sets the version recorded in the generated function table;
// it defaults to zero when omitted.
struct version_t {
    interface_version_t value;
};

consteval version_t version(interface_version_t value) {
    return {value};
}

// [[=since(n, impl)]] marks a method as introduced in version n. `impl` is
// called as a fallback when the function table's version is older than n.
struct since_t {
    interface_version_t version;
    std::meta::info default_impl;
};

template<class Func>
consteval since_t since(interface_version_t version, Func impl) {
    return {version, std::meta::reflect_constant(impl)};
}

// [[=adaptor(func)]] specifies the adaptor function used to invoke the method in the
// generated function table.
struct adaptor_t {
    std::meta::info adaptor_func;
};

template<class Func>
consteval adaptor_t adaptor(Func func) {
    return {std::meta::reflect_constant(func)};
}

namespace detail {
template<class I>
concept is_spec = !meta::annotations_of_with_type(^^I, ^^spec_t).empty();

inline consteval interface_version_t get_interface_version(meta::info I) {
    auto annotations = meta::annotations_of_with_type(I, ^^version_t);
    return annotations.empty() ? 0 : meta::extract<version_t>(annotations[0]).value;
}

inline consteval since_t get_method_since(meta::info method) {
    auto annotations = meta::annotations_of_with_type(method, ^^since_t);
    return annotations.empty() ? since_t{} : meta::extract<since_t>(annotations[0]);
}

inline consteval meta::info get_adaptor_func(meta::info method) {
    auto annotations = meta::annotations_of_with_type(method, ^^adaptor_t);
    return annotations.empty() ? meta::info{} : meta::extract<adaptor_t>(annotations[0]).adaptor_func;
}

template<class R, class... A>
using function_signature_t = R(A...);

inline consteval meta::info method_signature(meta::info method) {
    std::vector<meta::info> components = {meta::return_type_of(method)};
    for (meta::info parameter : meta::parameters_of(method)) {
        components.push_back(meta::type_of(parameter));
    }
    return meta::dealias(meta::substitute(^^function_signature_t, components));
}

static consteval bool is_static_candidate_function(meta::info candidate) {
    return meta::is_function(candidate) && meta::is_static_member(candidate);
}

static consteval bool is_member_candidate_function(meta::info candidate) {
    return meta::is_function(candidate) &&
           !meta::is_constructor(candidate) &&
           !meta::is_destructor(candidate) &&
           !meta::is_static_member(candidate);
}

static consteval void collect_static_candidates(
    meta::info implementation, std::vector<meta::info> &out) {
    for (meta::info candidate : meta::members_of(implementation, pub_ctx())) {
        if (is_static_candidate_function(candidate)) {
            out.push_back(candidate);
        }
    }
    for (meta::info base : meta::bases_of(implementation, pub_ctx())) {
        collect_static_candidates(meta::type_of(base), out);
    }
}

static consteval void collect_member_candidates(
    meta::info implementation, std::vector<meta::info> &out) {
    for (meta::info candidate : meta::members_of(implementation, pub_ctx())) {
        if (is_member_candidate_function(candidate)) {
            out.push_back(candidate);
        }
    }
    for (meta::info base : meta::bases_of(implementation, pub_ctx())) {
        collect_member_candidates(meta::type_of(base), out);
    }
}

template<class Data>
static constexpr std::span<meta::info const> static_candidates_v = [] {
    std::vector<meta::info> out;
    collect_static_candidates(^^Data, out);
    return std::define_static_array(out);
}();

template<class Data>
static constexpr std::span<meta::info const> member_candidates_v = [] {
    std::vector<meta::info> out;
    collect_member_candidates(^^Data, out);
    return std::define_static_array(out);
}();

template<class I>
struct default_def {
    using interface_handle_type = interface_handle<I>;

    struct vtable_type;

    static constexpr interface_version_t interface_version = get_interface_version(^^I);

    enum class field_kind {
        base,
        method,
    };

    struct field_descriptor {
        field_kind kind;
        meta::info spec;
    };

    static constexpr std::span<field_descriptor const> field_descriptors = [] {
        if (!meta::is_class_type(^^I)) {
            compile_time_error("facet: interface spec must be a class type");
        }
        std::vector<field_descriptor> result;
        for (meta::info base : meta::bases_of(^^I, all_ctx())) {
            result.emplace_back(field_kind::base, base);
        }
        for (meta::info member : meta::members_of(^^I, all_ctx())) {
            if (meta::is_special_member_function(member)) {
                continue;
            }
            if (!meta::is_function(member) ||
                meta::is_constructor(member) ||
                meta::is_destructor(member) ||
                meta::is_static_member(member) ||
                meta::is_operator_function(member) ||
                meta::is_conversion_function(member) ||
                meta::is_function_template(member)) {
                compile_time_error("facet: interface spec contains invalid member function");
            }
            result.emplace_back(field_kind::method, member);
        }
        return std::define_static_array(result);
    }();

    template<meta::info Base,
             class Type = typename[:meta::type_of(Base):]>
    struct base_pointer {
        using type = vtable_t<Type> const *;
    };

    template<meta::info Base>
    using base_pointer_t = typename base_pointer<Base>::type;

    template<meta::info Method,
             class Sig = typename[:method_signature(Method):]>
    struct method_pointer;

    template<meta::info Method, class R, class... A>
    struct method_pointer<Method, R(A...)> {
        using type = R (*)(interface_handle_type, A...);
    };

    template<meta::info Method>
    using method_pointer_t = method_pointer<Method>::type;

    consteval {
        std::vector<meta::info> members = {
            meta::data_member_spec(^^interface_version_t, {.name = "version"}),
        };
        for (auto desc : field_descriptors) {
            if (desc.kind == field_kind::base) {
                members.push_back(meta::data_member_spec(
                    meta::dealias(meta::substitute(
                        ^^base_pointer_t,
                        {
                            meta::reflect_constant(desc.spec),
                        })),
                    {.name = "_"}));
            }
            if (desc.kind == field_kind::method) {
                members.push_back(meta::data_member_spec(
                    meta::dealias(meta::substitute(
                        ^^method_pointer_t,
                        {
                            meta::reflect_constant(desc.spec),
                        })),
                    {.name = "_"}));
            }
        }
        meta::define_aggregate(^^vtable_type, members);
    }

    static constexpr std::span<meta::info const> fields_in_vtable = all_data_members_v<vtable_type>.subspan(1);

    template<auto... Values>
    static constexpr vtable_type value_with = {
        interface_version,
        Values...,
    };

    template<auto... Values>
    static constexpr vtable_type const *pointer_with = &value_with<Values...>;

    template<meta::info Base, meta::info Member,
             class Type = typename[:meta::type_of(Base):]>
    struct automatic_base_converter_t {
        constexpr vtable_t<Type> const *operator()(vtable_type const *vt) const noexcept {
            return vt->[:Member:];
        }
    };

    template<meta::info Base, meta::info Member>
    static constexpr automatic_base_converter_t<Base, Member> automatic_base_converter{};

    template<meta::info Method, meta::info Member,
             class Sig = typename[:method_signature(Method):]>
    struct automatic_method_invoker_t;

    template<meta::info Method, meta::info Member, class R, class... A>
    struct automatic_method_invoker_t<Method, Member, R(A...)> {
        R operator()(interface_handle_type handle, A... args) const {
            vtable_type const *vt = fct::get<0>(handle.vtables);
            constexpr since_t since_info = get_method_since(Method);
            if constexpr (since_info.default_impl != meta::info{}) {
                if (vt->version < since_info.version) {
                    return [:since_info.default_impl:](handle, std::forward<A>(args)...);
                }
            }
            return vt->[:Member:](handle, std::forward<A>(args)...);
        }
    };

    template<meta::info Method, meta::info Member>
    static constexpr automatic_method_invoker_t<Method, Member> automatic_method_invoker{};

    struct direct_members_t {
        std::span<direct_base_t const> bases;
        std::span<direct_method_t const> methods;
    };

    static constexpr direct_members_t direct_members = [] {
        std::vector<direct_base_t> bases;
        std::vector<direct_method_t> methods;
        for (auto [desc, member] : std::views::zip(field_descriptors, fields_in_vtable)) {
            if (desc.kind == field_kind::base) {
                bases.emplace_back(
                    meta::type_of(desc.spec),
                    meta::substitute(
                        ^^automatic_base_converter,
                        {
                            meta::reflect_constant(desc.spec),
                            meta::reflect_constant(member),
                        }));
            }
            if (desc.kind == field_kind::method) {
                methods.emplace_back(
                    static_string::make(meta::identifier_of(desc.spec)),
                    meta::substitute(
                        ^^automatic_method_invoker,
                        {
                            meta::reflect_constant(desc.spec),
                            meta::reflect_constant(member),
                        }));
            }
        }
        return direct_members_t{
            std::define_static_array(bases),
            std::define_static_array(methods),
        };
    }();

    template<class Data>
    struct vtable_implements {
        using object_handle_type = object_handle<Data, I>;

        template<meta::info Base,
                 class Type = typename[:meta::type_of(Base):]>
            requires(implements<Data, Type>)
        struct base_pointer {
            static constexpr vtable_t<Type> const *value = vtable_implements_v<Data, Type>;
        };

        template<meta::info Base>
            requires requires { base_pointer<Base>::value; }
        static constexpr auto base_pointer_v = base_pointer<Base>::value;

        template<meta::info Method,
                 class Sig = typename[:method_signature(Method):]>
        struct method_pointer;

        template<meta::info Method, class R, class... A>
        struct method_pointer<Method, R(A...)> {
            static consteval bool is_qualified_name(meta::info candidate) {
                return meta::has_identifier(candidate) &&
                       meta::identifier_of(candidate) == meta::identifier_of(Method);
            }

            template<meta::info Candidate>
                requires requires(object_handle_type handle, A... args) {
                    { [:Candidate:](handle, std::forward<A>(args)...) } -> std::convertible_to<R>;
                }
            static constexpr auto static_invoke_adaptor = [](object_handle_type handle, A... args) -> R {
                return [:Candidate:](handle, std::forward<A>(args)...);
            };

            static consteval bool is_static_exact(meta::info candidate) {
                auto candidate_params = meta::parameters_of(candidate);
                auto method_params = meta::parameters_of(Method);
                std::size_t i = 0;
                std::size_t j = 0;
                if (i >= candidate_params.size() ||
                    !meta::is_same_type(
                        meta::type_of(candidate_params[i++]),
                        ^^object_handle_type)) {
                    return false;
                }
                while (i < candidate_params.size() && j < method_params.size()) {
                    if (!meta::is_same_type(
                            meta::type_of(candidate_params[i++]),
                            meta::type_of(method_params[j++]))) {
                        return false;
                    }
                }
                return i == candidate_params.size() && j == method_params.size();
            }

            template<meta::info Candidate>
                requires requires(object_handle_type handle, A... args) {
                    { handle.get()->[:Candidate:](std::forward<A>(args)...) } -> std::convertible_to<R>;
                }
            static constexpr auto member_invoke_adaptor = [](object_handle_type handle, A... args) -> R {
                return handle.get()->[:Candidate:](std::forward<A>(args)...);
            };

            static consteval bool is_member_exact(meta::info candidate) {
                auto candidate_params = meta::parameters_of(candidate);
                auto method_params = meta::parameters_of(Method);
                std::size_t i = 0;
                std::size_t j = 0;
                while (i < candidate_params.size() && j < method_params.size()) {
                    if (!meta::is_same_type(
                            meta::type_of(candidate_params[i++]),
                            meta::type_of(method_params[j++]))) {
                        return false;
                    }
                }
                return i == candidate_params.size() && j == method_params.size();
            }

            static constexpr meta::info adaptor = [] -> meta::info {
                auto adaptor_func = get_adaptor_func(Method);
                if (adaptor_func != meta::info{}) {
                    return adaptor_func;
                }
                std::vector<meta::info> exact_static_matches;
                std::vector<meta::info> exact_member_matches;
                std::vector<meta::info> static_matches;
                std::vector<meta::info> member_matches;
                for (meta::info candidate : static_candidates_v<Data>) {
                    if (is_qualified_name(candidate)) {
                        if (meta::can_substitute(^^static_invoke_adaptor, {meta::reflect_constant(candidate)})) {
                            auto adaptor = meta::substitute(^^static_invoke_adaptor, {meta::reflect_constant(candidate)});
                            if (is_static_exact(candidate)) {
                                exact_static_matches.push_back(adaptor);
                            } else {
                                static_matches.push_back(adaptor);
                            }
                        }
                    }
                }
                for (meta::info candidate : member_candidates_v<Data>) {
                    if (is_qualified_name(candidate)) {
                        if (meta::can_substitute(^^member_invoke_adaptor, {meta::reflect_constant(candidate)})) {
                            auto adaptor = meta::substitute(^^member_invoke_adaptor, {meta::reflect_constant(candidate)});
                            if (is_member_exact(candidate)) {
                                exact_member_matches.push_back(adaptor);
                            } else {
                                member_matches.push_back(adaptor);
                            }
                        }
                    }
                }
                for (meta::info match : exact_static_matches) {
                    return match;
                }
                for (meta::info match : exact_member_matches) {
                    return match;
                }
                for (meta::info match : static_matches) {
                    return match;
                }
                for (meta::info match : member_matches) {
                    return match;
                }
                return {};
            }();

            static R call(interface_handle_type handle, A... args)
                requires(adaptor != meta::info{})
            {
                object_handle_type object(static_cast<Data *>(handle.data), handle.rtti, handle.vtables);
                return [:adaptor:](object, std::forward<A>(args)...);
            }
        };

        template<meta::info Method>
            requires requires { &method_pointer<Method>::call; }
        static constexpr auto method_pointer_v = &method_pointer<Method>::call;

        static constexpr bool has_value = [] {
            for (auto desc : field_descriptors) {
                if (desc.kind == field_kind::base) {
                    if (!meta::can_substitute(
                            ^^base_pointer_v,
                            {
                                meta::reflect_constant(desc.spec),
                            })) {
                        return false;
                    }
                }
                if (desc.kind == field_kind::method) {
                    if (!meta::can_substitute(
                            ^^method_pointer_v,
                            {
                                meta::reflect_constant(desc.spec),
                            })) {
                        return false;
                    }
                }
            }
            return true;
        }();

        static constexpr vtable_type const *value = [] {
            std::vector<meta::info> members;
            for (auto desc : field_descriptors) {
                if (desc.kind == field_kind::base) {
                    members.push_back(
                        meta::dealias(meta::substitute(
                            ^^base_pointer_v,
                            {
                                meta::reflect_constant(desc.spec),
                            })));
                }
                if (desc.kind == field_kind::method) {
                    members.push_back(
                        meta::dealias(meta::substitute(
                            ^^method_pointer_v,
                            {
                                meta::reflect_constant(desc.spec),
                            })));
                }
            }
            return meta::extract<vtable_type const *>(meta::substitute(^^pointer_with, members));
        }();
    };
};
}

template<detail::is_spec I>
struct direct_methods<I> {
    static constexpr auto const &value = detail::default_def<I>::direct_members.methods;
};

template<detail::is_spec I>
struct direct_bases<I> {
    static constexpr auto const &value = detail::default_def<I>::direct_members.bases;
};

template<detail::is_spec I>
struct vtable<I> {
    using type = typename detail::default_def<I>::vtable_type;
};

template<class Data, detail::is_spec I>
    requires(detail::default_def<I>::template vtable_implements<Data>::has_value)
struct vtable_implements<Data, I> {
    static constexpr vtable_t<I> const *value = detail::default_def<I>::template vtable_implements<Data>::value;
};
}
