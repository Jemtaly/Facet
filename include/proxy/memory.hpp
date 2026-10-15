#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include <proxy.hpp>

// ============================================================================
// Ownership wrappers
//
// object_view is non-owning, object_holder shares ownership, and object_owner
// uniquely owns its implementation. The interface aliases expose these same
// wrappers without an implementation type.
// ============================================================================

namespace proxy {
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
        Impl data;

        template<class... Args>
        explicit type(Args &&...args) noexcept : shared_controller{.destroy = destroy}, data(std::forward<Args>(args)...) {}

        static void destroy(shared_controller *control) noexcept {
            delete static_cast<type *>(control);
        }

        Impl *get_data() noexcept {
            return &data;
        }
    };
};

template<class Impl>
using shared_controller_for = typename shared_controller_for_def<Impl>::type;
}

template<class Data, class... Is>
struct object_view;

template<class Data, class... Is>
struct object_holder;

template<class Data, class... Is>
struct object_view {
    using handle_type = object_handle<Data, Is...>;

    handle_type handle_;
    detail::shared_controller *controller_ = nullptr;

    constexpr object_view() noexcept = default;

    object_view(handle_type handle, detail::shared_controller *controller) noexcept : handle_(handle), controller_(controller) {}

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...> && !std::is_same_v<object_view<Base, Js...>, object_view>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator object_view<Base, Js...>() const & noexcept {
        return object_view<Base, Js...>(object_handle<Base, Js...>(handle_), controller_);
    }

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator object_holder<Base, Js...>() const & noexcept {
        return object_holder<Base, Js...>(object_handle<Base, Js...>(handle_), detail::add_ref(controller_));
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

template<class Data, class... Is>
struct object_holder {
    using handle_type = object_handle<Data, Is...>;

    handle_type handle_;
    detail::shared_controller *controller_ = nullptr;

    constexpr object_holder() noexcept = default;

    object_holder(handle_type handle, detail::shared_controller *controller) noexcept : handle_(handle), controller_(controller) {}

    object_holder(object_holder const &other) noexcept : handle_(other.handle_), controller_(detail::add_ref(other.controller_)) {}

    object_holder(object_holder &&other) noexcept : handle_(other.handle_), controller_(std::exchange(other.controller_, nullptr)) {}

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...> && !std::is_same_v<object_holder<Base, Js...>, object_holder>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator object_holder<Base, Js...>() const & noexcept {
        return object_holder<Base, Js...>(object_handle<Base, Js...>(handle_), detail::add_ref(controller_));
    }

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...> && !std::is_same_v<object_holder<Base, Js...>, object_holder>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator object_holder<Base, Js...>() && noexcept {
        return object_holder<Base, Js...>(object_handle<Base, Js...>(handle_), std::exchange(controller_, nullptr));
    }

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator object_view<Base, Js...>() const & noexcept {
        return object_view<Base, Js...>(object_handle<Base, Js...>(handle_), controller_);
    }

    ~object_holder() {
        detail::release(controller_);
    }

    void swap(object_holder &other) noexcept {
        std::swap(handle_, other.handle_);
        std::swap(controller_, other.controller_);
    }

    object_holder &operator=(object_holder other) noexcept {
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
using interface_view = object_view<void, Is...>;

using data_view = interface_view<>;

template<class... Is>
using interface_holder = object_holder<void, Is...>;

using data_holder = interface_holder<>;

template<class Impl, class... Is, class... Args>
auto make_shared(Args &&...args) {
    auto *controller = new detail::shared_controller_for<Impl>(std::forward<Args>(args)...);
    return object_holder<Impl, Is...>(static_policy<Impl, Is...>::make_handle(controller->get_data()), controller);
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
        Impl data;

        template<class... Args>
        explicit type(Args &&...args) noexcept : shared_controller{.destroy = destroy}, data(std::forward<Args>(args)...) {
            data.controller_ = this;
        }

        static void destroy(shared_controller *control) noexcept {
            delete static_cast<type *>(control);
        }

        Impl *get_data() noexcept {
            return &data;
        }
    };
};
}

template<class Impl, class... Is>
    requires(std::derived_from<Impl, enable_shared_from_this<Impl>>)
auto recover_shared(object_handle<Impl, Is...> handle) {
    return object_view<Impl, Is...>(handle, handle.get()->get_controller());
}
}

namespace proxy {
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
        Impl data;

        template<class... Args>
        explicit type(Args &&...args) noexcept : unique_controller{.destroy = destroy}, data(std::forward<Args>(args)...) {}

        static void destroy(unique_controller *control) noexcept {
            delete static_cast<type *>(control);
        }

        Impl *get_data() noexcept {
            return &data;
        }
    };
};

template<class Impl>
using unique_controller_for = typename unique_controller_for_def<Impl>::type;
}

template<class Data, class... Is>
struct object_owner {
    using handle_type = object_handle<Data, Is...>;

    handle_type handle_;
    detail::unique_controller *controller_ = nullptr;

    constexpr object_owner() noexcept = default;

    object_owner(handle_type handle, detail::unique_controller *controller) noexcept : handle_(handle), controller_(controller) {}

    object_owner(object_owner const &) = delete;

    object_owner(object_owner &&other) noexcept : handle_(other.handle_), controller_(std::exchange(other.controller_, nullptr)) {}

    template<class Base, class... Js>
        requires(handle_type::template can_convert_to<Base, Js...> && !std::is_same_v<object_owner<Base, Js...>, object_owner>)
    explicit(!handle_type::template can_implicitly_convert_to<Js...>)
    operator object_owner<Base, Js...>() && noexcept {
        return object_owner<Base, Js...>(object_handle<Base, Js...>(handle_), std::exchange(controller_, nullptr));
    }

    ~object_owner() {
        detail::release(controller_);
    }

    void swap(object_owner &other) noexcept {
        std::swap(handle_, other.handle_);
        std::swap(controller_, other.controller_);
    }

    object_owner &operator=(object_owner other) noexcept {
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
using interface_owner = object_owner<void, Is...>;

using data_owner = interface_owner<>;

template<class Impl, class... Is, class... Args>
auto make_unique(Args &&...args) {
    auto *controller = new detail::unique_controller_for<Impl>(std::forward<Args>(args)...);
    return object_owner<Impl, Is...>(static_policy<Impl, Is...>::make_handle(controller->get_data()), controller);
}
}
