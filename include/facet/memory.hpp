#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include <facet.hpp>

// ============================================================================
// Ownership wrappers
//
// weak_typed_facets is non-owning, shared_typed_facets shares ownership, and unique_typed_facets
// uniquely owns its implementation. The interface aliases expose these same
// wrappers without an implementation type.
// ============================================================================

namespace fct {
namespace detail {
using ref_count_t = std::uint32_t;

struct shared_controller {
    ref_count_t references = 1;
    void (*destroy)(shared_controller *) noexcept;
};

inline shared_controller *add_ref(shared_controller *control) noexcept {
    if (control != nullptr) {
        std::atomic_ref<ref_count_t>(control->references).fetch_add(1, std::memory_order_relaxed);
    }
    return control;
}

inline void release(shared_controller *control) noexcept {
    if (control != nullptr) {
        if (std::atomic_ref<ref_count_t>(control->references).fetch_sub(1, std::memory_order_acq_rel) == 1) {
            control->destroy(control);
        }
    }
}

template<class Impl>
struct shared_controller_for_def {
    struct type : shared_controller {
        Impl impl;

        template<class... Args>
        explicit type(Args &&...args) noexcept : shared_controller{.destroy = destroy}, impl(std::forward<Args>(args)...) {}

        static void destroy(shared_controller *control) noexcept {
            delete static_cast<type *>(control);
        }

        Impl *get_impl() noexcept {
            return &impl;
        }
    };
};

template<class Impl>
using shared_controller_for = typename shared_controller_for_def<Impl>::type;
}

template<class Impl, class... Is>
struct weak_typed_facets;

template<class Impl, class... Is>
struct shared_typed_facets;

template<class Impl, class... Is>
struct weak_typed_facets {
    using handle_type = typed_facets<Impl, Is...>;

    handle_type handle_;
    detail::shared_controller *controller_ = nullptr;

    constexpr weak_typed_facets() noexcept = default;

    weak_typed_facets(handle_type handle, detail::shared_controller *controller) noexcept : handle_(handle), controller_(controller) {}

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...> && !std::is_same_v<weak_typed_facets<Base, Js...>, weak_typed_facets>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator weak_typed_facets<Base, Js...>() const & noexcept {
        return weak_typed_facets<Base, Js...>(typed_facets<Base, Js...>(handle_), controller_);
    }

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator shared_typed_facets<Base, Js...>() const & noexcept {
        return shared_typed_facets<Base, Js...>(typed_facets<Base, Js...>(handle_), detail::add_ref(controller_));
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(handle_) && controller_ != nullptr;
    }

    auto &operator*() const noexcept {
        return *handle_;
    }

    auto *operator->() const noexcept {
        return handle_.operator->();
    }

    template<class I>
    auto *as() const noexcept {
        return handle_.template as<I>();
    }

    auto *get() const noexcept {
        return handle_.get();
    }

    auto get_handle() const noexcept {
        return handle_;
    }
};

template<class Impl, class... Is>
struct shared_typed_facets {
    using handle_type = typed_facets<Impl, Is...>;

    handle_type handle_;
    detail::shared_controller *controller_ = nullptr;

    constexpr shared_typed_facets() noexcept = default;

    shared_typed_facets(handle_type handle, detail::shared_controller *controller) noexcept : handle_(handle), controller_(controller) {}

    shared_typed_facets(shared_typed_facets const &other) noexcept : handle_(other.handle_), controller_(detail::add_ref(other.controller_)) {}

    shared_typed_facets(shared_typed_facets &&other) noexcept : handle_(other.handle_), controller_(std::exchange(other.controller_, nullptr)) {}

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...> && !std::is_same_v<shared_typed_facets<Base, Js...>, shared_typed_facets>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator shared_typed_facets<Base, Js...>() const & noexcept {
        return shared_typed_facets<Base, Js...>(typed_facets<Base, Js...>(handle_), detail::add_ref(controller_));
    }

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...> && !std::is_same_v<shared_typed_facets<Base, Js...>, shared_typed_facets>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator shared_typed_facets<Base, Js...>() && noexcept {
        return shared_typed_facets<Base, Js...>(typed_facets<Base, Js...>(handle_), std::exchange(controller_, nullptr));
    }

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator weak_typed_facets<Base, Js...>() const & noexcept {
        return weak_typed_facets<Base, Js...>(typed_facets<Base, Js...>(handle_), controller_);
    }

    ~shared_typed_facets() {
        detail::release(controller_);
    }

    void swap(shared_typed_facets &other) noexcept {
        std::swap(handle_, other.handle_);
        std::swap(controller_, other.controller_);
    }

    shared_typed_facets &operator=(shared_typed_facets other) noexcept {
        swap(other);
        return *this;
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(handle_) && controller_ != nullptr;
    }

    auto &operator*() const noexcept {
        return *handle_;
    }

    auto *operator->() const noexcept {
        return handle_.operator->();
    }

    template<class I>
    auto *as() const noexcept {
        return handle_.template as<I>();
    }

    auto *get() const noexcept {
        return handle_.get();
    }

    auto get_handle() const noexcept {
        return handle_;
    }
};

template<class... Is>
using weak_facets = weak_typed_facets<void, Is...>;

using weak_opaque = weak_facets<>;

template<class... Is>
using shared_facets = shared_typed_facets<void, Is...>;

using shared_opaque = shared_facets<>;

template<class Impl, class... Is, class... Args>
auto make_shared(Args &&...args) {
    auto *controller = new detail::shared_controller_for<Impl>(std::forward<Args>(args)...);
    return shared_typed_facets<Impl, Is...>(as_facets<Is...>(controller->get_impl()), controller);
}

template<class Self>
struct enable_shared_from_this {
    detail::shared_controller *controller_ = nullptr;

    enable_shared_from_this() = default;

    detail::shared_controller *get_controller() noexcept {
        return controller_;
    }
};

namespace detail {
template<class Impl>
    requires(std::derived_from<Impl, enable_shared_from_this<Impl>>)
struct shared_controller_for_def<Impl> {
    struct type : shared_controller {
        Impl impl;

        template<class... Args>
        explicit type(Args &&...args) noexcept : shared_controller{.destroy = destroy}, impl(std::forward<Args>(args)...) {
            impl.controller_ = this;
        }

        static void destroy(shared_controller *control) noexcept {
            delete static_cast<type *>(control);
        }

        Impl *get_impl() noexcept {
            return &impl;
        }
    };
};
}

template<class Impl, class... Is>
    requires(std::derived_from<Impl, enable_shared_from_this<Impl>>)
auto retain_weak(typed_facets<Impl, Is...> handle) {
    return weak_typed_facets<Impl, Is...>(handle, handle.get()->get_controller());
}

template<class Impl, class... Is>
auto retain_shared(typed_facets<Impl, Is...> handle) {
    return shared_typed_facets<Impl, Is...>(handle, detail::add_ref(handle.get()->get_controller()));
}
}

namespace fct {
namespace detail {
struct unique_controller {
    void (*destroy)(unique_controller *) noexcept;
};

inline void release(unique_controller *control) noexcept {
    if (control != nullptr) {
        control->destroy(control);
    }
}

template<class Impl>
struct unique_controller_for_def {
    struct type : unique_controller {
        Impl impl;

        template<class... Args>
        explicit type(Args &&...args) noexcept : unique_controller{.destroy = destroy}, impl(std::forward<Args>(args)...) {}

        static void destroy(unique_controller *control) noexcept {
            delete static_cast<type *>(control);
        }

        Impl *get_impl() noexcept {
            return &impl;
        }
    };
};

template<class Impl>
using unique_controller_for = typename unique_controller_for_def<Impl>::type;
}

template<class Impl, class... Is>
struct unique_typed_facets {
    using handle_type = typed_facets<Impl, Is...>;

    handle_type handle_;
    detail::unique_controller *controller_ = nullptr;

    constexpr unique_typed_facets() noexcept = default;

    unique_typed_facets(handle_type handle, detail::unique_controller *controller) noexcept : handle_(handle), controller_(controller) {}

    unique_typed_facets(unique_typed_facets const &) = delete;

    unique_typed_facets(unique_typed_facets &&other) noexcept : handle_(other.handle_), controller_(std::exchange(other.controller_, nullptr)) {}

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...> && !std::is_same_v<unique_typed_facets<Base, Js...>, unique_typed_facets>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator unique_typed_facets<Base, Js...>() && noexcept {
        return unique_typed_facets<Base, Js...>(typed_facets<Base, Js...>(handle_), std::exchange(controller_, nullptr));
    }

    ~unique_typed_facets() {
        detail::release(controller_);
    }

    void swap(unique_typed_facets &other) noexcept {
        std::swap(handle_, other.handle_);
        std::swap(controller_, other.controller_);
    }

    unique_typed_facets &operator=(unique_typed_facets other) noexcept {
        swap(other);
        return *this;
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(handle_) && controller_ != nullptr;
    }

    auto &operator*() const noexcept {
        return *handle_;
    }

    auto *operator->() const noexcept {
        return handle_.operator->();
    }

    template<class I>
    auto *as() const noexcept {
        return handle_.template as<I>();
    }

    auto *get() const noexcept {
        return handle_.get();
    }

    auto get_handle() const noexcept {
        return handle_;
    }
};

template<class... Is>
using unique_facets = unique_typed_facets<void, Is...>;

using unique_opaque = unique_facets<>;

template<class Impl, class... Is, class... Args>
auto make_unique(Args &&...args) {
    auto *controller = new detail::unique_controller_for<Impl>(std::forward<Args>(args)...);
    return unique_typed_facets<Impl, Is...>(as_facets<Is...>(controller->get_impl()), controller);
}
}
