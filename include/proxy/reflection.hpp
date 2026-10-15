#pragma once

#include <cstddef>
#include <cstdint>
#include <meta>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <proxy.hpp>

// ============================================================================
// Annotations
// ============================================================================

namespace proxy {
// [[=uuid("...")]] assigns a non-empty id used to find the interface during
// runtime cross-casts.
struct uuid_t {
    fixed_string value;
};

consteval uuid_t uuid(std::string_view value) {
    return {make_fixed_string(value)};
}

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

template<class Candidate>
consteval since_t since(interface_version_t version, Candidate impl) {
    return {version, std::meta::reflect_constant(impl)};
}

namespace detail {
template<class I>
concept has_uuid = !meta::annotations_of_with_type(^^I, ^^uuid_t).empty();

inline consteval interface_id_t get_interface_id(meta::info I) {
    auto annotations = meta::annotations_of_with_type(I, ^^uuid_t);
    return std::define_static_string(meta::extract<uuid_t>(annotations[0]).value.view());
}

inline consteval interface_version_t get_interface_version(meta::info I) {
    auto annotations = meta::annotations_of_with_type(I, ^^version_t);
    return annotations.empty() ? 0 : meta::extract<version_t>(annotations[0]).value;
}

inline consteval since_t get_method_since(meta::info method) {
    auto annotations = meta::annotations_of_with_type(method, ^^since_t);
    return annotations.empty() ? since_t{} : meta::extract<since_t>(annotations[0]);
}

inline consteval std::string to_decimal_string(std::size_t n) {
    std::string s;
    do {
        s.insert(s.begin(), static_cast<char>('0' + n % 10));
        n /= 10;
    } while (n != 0);
    return s;
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

template<class I>
struct default_def {
    using interface_handle_type = interface_handle<I>;

    static consteval std::vector<meta::info> collect_spec_methods() {
        if (!meta::is_class_type(^^I)) {
            compile_time_error("proxy: interface spec must be a class type");
        }
        std::vector<meta::info> result;
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
                compile_time_error("proxy: interface spec contains invalid member function");
            }
            result.push_back(member);
        }
        return result;
    }

    static consteval std::vector<meta::info> collect_spec_bases() {
        std::vector<meta::info> result;
        for (meta::info base : meta::bases_of(^^I, all_ctx())) {
            result.push_back(base);
        }
        return result;
    }

    static constexpr std::span<meta::info const> spec_methods =
        std::define_static_array(collect_spec_methods());

    static constexpr std::span<meta::info const> spec_bases =
        std::define_static_array(collect_spec_bases());

    static constexpr interface_version_t interface_version = get_interface_version(^^I);

    struct ftable_type;

    struct vtable_type;

    struct abi_type {
        void *data;
        type_info const *rtti;
        vtable_type const *vt;
    };

    template<meta::info Method,
             class Sig = typename[:method_signature(Method):]>
    struct thunk_pointer;

    template<meta::info Method, class R, class... A>
    struct thunk_pointer<Method, R(A...)> {
        using type = R (*)(abi_type, A...);
    };

    template<meta::info Method>
    using thunk_pointer_t = thunk_pointer<Method>::type;

    consteval {
        std::vector<meta::info> members = {
            meta::data_member_spec(^^interface_version_t, {.name = "version"}),
        };
        std::size_t index = 0;
        for (meta::info method : spec_methods) {
            members.push_back(meta::data_member_spec(
                meta::dealias(meta::substitute(^^thunk_pointer_t, {meta::reflect_constant(method)})),
                {.name = "method_" + to_decimal_string(index++)}));
        }
        meta::define_aggregate(^^ftable_type, members);
    }

    template<meta::info Base,
             class Type = typename[:meta::type_of(Base):]>
    struct base_pointer {
        using type = vtable_t<Type> const *;
    };

    template<meta::info Base>
    using base_pointer_t = typename base_pointer<Base>::type;

    consteval {
        std::vector<meta::info> members = {
            meta::data_member_spec(^^ftable_type const *, {.name = "self"}),
        };
        std::size_t index = 0;
        for (meta::info base : spec_bases) {
            members.push_back(meta::data_member_spec(
                meta::dealias(meta::substitute(^^base_pointer_t, {meta::reflect_constant(base)})),
                {.name = "base_" + to_decimal_string(index++)}));
        }
        meta::define_aggregate(^^vtable_type, members);
    }

    static consteval meta::info vtable_base_member(std::size_t index) {
        return nth_data_member(^^vtable_type, index + 1);
    }

    static consteval meta::info ftable_method_member(std::size_t index) {
        return nth_data_member(^^ftable_type, index + 1);
    }

    template<meta::info Method, meta::info Member,
             class Sig = typename[:method_signature(Method):]>
    struct automatic_method_invoker_t;

    template<meta::info Method, meta::info Member, class R, class... A>
    struct automatic_method_invoker_t<Method, Member, R(A...)> {
        R operator()(interface_handle_type handle, A... args) const {
            vtable_type const *vt = std::get<0>(handle.vtables);
            ftable_type const *ft = vt->self;
            constexpr since_t since_info = get_method_since(Method);
            if constexpr (since_info.default_impl != meta::info{}) {
                if (ft->version < since_info.version) {
                    return [:since_info.default_impl:](handle, std::forward<A>(args)...);
                }
            }
            abi_type abi{handle.data, handle.rtti, vt};
            return ft->[:Member:](abi, std::forward<A>(args)...);
        }
    };

    template<meta::info Method, meta::info Member>
    static constexpr automatic_method_invoker_t<Method, Member> automatic_method_invoker{};

    static consteval std::vector<direct_method_t> make_direct_methods() {
        std::vector<direct_method_t> result;
        template for (constexpr std::size_t index : std::views::iota(0uz, spec_methods.size())) {
            constexpr meta::info method = spec_methods[index];
            constexpr meta::info member = ftable_method_member(index);
            result.push_back(direct_method_t{
                make_fixed_string(meta::identifier_of(method)),
                meta::reflect_constant(automatic_method_invoker<method, member>),
            });
        }
        return result;
    }

    template<meta::info Base, meta::info Member,
             class Type = typename[:meta::type_of(Base):]>
    struct automatic_base_converter_t {
        constexpr vtable_t<Type> const *operator()(vtable_type const *table) const noexcept {
            return table->[:Member:];
        }
    };

    template<meta::info Base, meta::info Member>
    static constexpr automatic_base_converter_t<Base, Member> automatic_base_converter{};

    static consteval std::vector<direct_base_t> make_direct_bases() {
        std::vector<direct_base_t> result;
        template for (constexpr std::size_t index : std::views::iota(0uz, spec_bases.size())) {
            constexpr meta::info base = spec_bases[index];
            constexpr meta::info member = vtable_base_member(index);
            result.push_back(direct_base_t{
                meta::type_of(base),
                meta::reflect_constant(automatic_base_converter<base, member>),
            });
        }
        return result;
    }

    struct direct_methods {
        static constexpr std::span<direct_method_t const> value =
            std::define_static_array(make_direct_methods());
    };

    struct direct_bases {
        static constexpr std::span<direct_base_t const> value =
            std::define_static_array(make_direct_bases());
    };

    template<class Data>
    struct vtable_implements {
        using object_handle_type = object_handle<Data, I>;

        template<meta::info Candidate, class R, class... A>
        static constexpr bool static_impl_r_v = requires(object_handle_type handle, A... args) {
            { [:Candidate:](handle, std::forward<A>(args)...) } -> std::convertible_to<R>;
        };

        template<meta::info Candidate, class R, class... A>
        static constexpr bool member_impl_r_v = requires(object_handle_type handle, A... args) {
            { handle.get()->[:Candidate:](std::forward<A>(args)...) } -> std::convertible_to<R>;
        };

        template<meta::info Candidate, class R, class... A>
        struct static_invoke_adaptor {
            object_handle_type handle;

            R operator()(A... args) const {
                return [:Candidate:](handle, std::forward<A>(args)...);
            }
        };

        template<meta::info Candidate, class R, class... A>
        struct member_invoke_adaptor {
            object_handle_type handle;

            R operator()(A... args) const {
                return handle.get()->[:Candidate:](std::forward<A>(args)...);
            }
        };

        static consteval bool is_static_candidate_function(meta::info candidate) {
            return meta::is_function(candidate) && meta::is_static_member(candidate);
        }

        static consteval bool is_member_candidate_function(meta::info candidate) {
            return meta::is_function(candidate) &&
                   !meta::is_constructor(candidate) &&
                   !meta::is_destructor(candidate) &&
                   !meta::is_static_member(candidate);
        }

        static consteval std::string implemented_name_of(meta::info function) {
            return meta::has_identifier(function)
                       ? std::string(meta::identifier_of(function))
                       : std::string{};
        }

        static consteval void collect_static_candidates(
            meta::info implementation, std::string_view name, std::vector<meta::info> &out) {
            for (meta::info candidate : meta::members_of(implementation, pub_ctx())) {
                if (is_static_candidate_function(candidate) &&
                    implemented_name_of(candidate) == name) {
                    out.push_back(candidate);
                }
            }
            for (meta::info base : meta::bases_of(implementation, pub_ctx())) {
                collect_static_candidates(meta::type_of(base), name, out);
            }
        }

        static consteval void collect_member_candidates(
            meta::info implementation, std::string_view name, std::vector<meta::info> &out) {
            for (meta::info candidate : meta::members_of(implementation, pub_ctx())) {
                if (is_member_candidate_function(candidate) &&
                    implemented_name_of(candidate) == name) {
                    out.push_back(candidate);
                }
            }
            for (meta::info base : meta::bases_of(implementation, pub_ctx())) {
                collect_member_candidates(meta::type_of(base), name, out);
            }
        }

        static consteval std::vector<meta::info> invocability_args(
            meta::info candidate, meta::info method) {
            std::vector<meta::info> args = {
                meta::reflect_constant(candidate),
                meta::return_type_of(method),
            };
            for (meta::info parameter : meta::parameters_of(method)) {
                args.push_back(meta::type_of(parameter));
            }
            return args;
        }

        static consteval bool static_candidate_impl(meta::info candidate, meta::info method) {
            return meta::extract<bool>(meta::substitute(
                ^^static_impl_r_v, invocability_args(candidate, method)));
        }

        static consteval bool static_candidate_exact(meta::info candidate, meta::info method) {
            if (!static_candidate_impl(candidate, method)) {
                return false;
            }
            auto candidate_params = meta::parameters_of(candidate);
            auto method_params = meta::parameters_of(method);
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

        static consteval bool member_candidate_impl(meta::info candidate, meta::info method) {
            return meta::extract<bool>(meta::substitute(
                ^^member_impl_r_v, invocability_args(candidate, method)));
        }

        static consteval bool member_candidate_exact(meta::info candidate, meta::info method) {
            if (!member_candidate_impl(candidate, method)) {
                return false;
            }
            auto candidate_params = meta::parameters_of(candidate);
            auto method_params = meta::parameters_of(method);
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

        static consteval meta::info resolve_impl(meta::info method) {
            std::vector<meta::info> static_candidates;
            std::vector<meta::info> member_candidates;
            collect_static_candidates(^^Data, meta::identifier_of(method), static_candidates);
            collect_member_candidates(^^Data, meta::identifier_of(method), member_candidates);
            for (meta::info candidate : static_candidates) {
                if (static_candidate_exact(candidate, method)) {
                    return meta::dealias(meta::substitute(
                        ^^static_invoke_adaptor, invocability_args(candidate, method)));
                }
            }
            for (meta::info candidate : member_candidates) {
                if (member_candidate_exact(candidate, method)) {
                    return meta::dealias(meta::substitute(
                        ^^member_invoke_adaptor, invocability_args(candidate, method)));
                }
            }
            for (meta::info candidate : static_candidates) {
                if (static_candidate_impl(candidate, method)) {
                    return meta::dealias(meta::substitute(
                        ^^static_invoke_adaptor, invocability_args(candidate, method)));
                }
            }
            for (meta::info candidate : member_candidates) {
                if (member_candidate_impl(candidate, method)) {
                    return meta::dealias(meta::substitute(
                        ^^member_invoke_adaptor, invocability_args(candidate, method)));
                }
            }
            return {};
        }

        template<meta::info Method,
                 class Sig = typename[:method_signature(Method):]>
        struct thunk_pointer;

        template<meta::info Method, class R, class... A>
        struct thunk_pointer<Method, R(A...)> {
            static R call(abi_type abi, A... args) {
                constexpr meta::info adaptor = resolve_impl(Method);
                if constexpr (adaptor == meta::info{}) {
                    static_assert(false, "proxy: no suitable implementation found");
                } else {
                    object_handle_type handle(static_cast<Data *>(abi.data), abi.rtti, abi.vt);
                    return typename[:adaptor:](handle)(std::forward<A>(args)...);
                }
            }
        };

        template<meta::info Method>
        static constexpr auto thunk_pointer_v = &thunk_pointer<Method>::call;

        static constexpr ftable_type ftable_value = []<std::size_t... K>(std::index_sequence<K...>) {
            return ftable_type{
                interface_version,
                thunk_pointer_v<spec_methods[K]>...,
            };
        }(std::make_index_sequence<spec_methods.size()>{});

        template<meta::info Base,
                 class Type = typename[:meta::type_of(Base):]>
        struct base_pointer {
            static constexpr vtable_t<Type> const *value = vtable_implements_v<Data, Type>;
        };

        template<meta::info Base>
        static constexpr auto base_pointer_v = base_pointer<Base>::value;

        static constexpr vtable_type vtable_value = []<std::size_t... K>(std::index_sequence<K...>) {
            return vtable_type{
                &ftable_value,
                base_pointer_v<spec_bases[K]>...,
            };
        }(std::make_index_sequence<spec_bases.size()>{});

        static constexpr vtable_type const *value = &vtable_value;
    };
};
}

template<detail::has_uuid I>
struct interface_id<I> {
    static constexpr interface_id_t value = detail::get_interface_id(^^I);
};

template<class I>
struct direct_methods {
    static constexpr auto const &value = detail::default_def<I>::direct_methods::value;
};

template<class I>
struct direct_bases {
    static constexpr auto const &value = detail::default_def<I>::direct_bases::value;
};

template<class I>
struct vtable {
    using type = typename detail::default_def<I>::vtable_type;
};

template<class Data, class I>
struct vtable_implements {
    static constexpr vtable_t<I> const *value = detail::default_def<I>::template vtable_implements<Data>::value;
};
}
