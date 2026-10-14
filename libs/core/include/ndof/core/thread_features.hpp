#ifndef NDOF_CORE_THREAD_FEATURES_HPP
#define NDOF_CORE_THREAD_FEATURES_HPP
#include "configs.hpp"

#if defined(NDOF_THREADS_FEATURE_ENABLED) && NDOF_THREADS_FEATURE_ENABLED == 1
#include <mutex>
#include <shared_mutex>
#endif

namespace ndof {

class no_thread_mutex {
public:
    constexpr no_thread_mutex() noexcept = default;
    constexpr ~no_thread_mutex() noexcept = default;
    constexpr no_thread_mutex(const no_thread_mutex&) noexcept = default;
    constexpr no_thread_mutex& operator=(const no_thread_mutex&) noexcept = default;
    constexpr no_thread_mutex(no_thread_mutex&&) noexcept = default;
    constexpr no_thread_mutex& operator=(no_thread_mutex&&) noexcept = default;

    constexpr void lock() noexcept {}
    constexpr bool try_lock() noexcept { return true; }
    constexpr void unlock() noexcept {}
};

class no_thread_shared_mutex {
public:
    constexpr no_thread_shared_mutex() noexcept = default;
    constexpr ~no_thread_shared_mutex() noexcept = default;
    constexpr no_thread_shared_mutex(const no_thread_shared_mutex&) noexcept = default;
    constexpr no_thread_shared_mutex& operator=(const no_thread_shared_mutex&) noexcept = default;
    constexpr no_thread_shared_mutex(no_thread_shared_mutex&&) noexcept = default;
    constexpr no_thread_shared_mutex& operator=(no_thread_shared_mutex&&) noexcept = default;

    constexpr void lock() noexcept {}
    constexpr bool try_lock() noexcept { return true; }
    constexpr void unlock() noexcept {}
    constexpr void lock_shared() noexcept {}
    constexpr bool try_lock_shared() noexcept { return true; }
    constexpr void unlock_shared() noexcept {}
};

template <typename Mutex>
class no_thread_lock_guard {
public:
    no_thread_lock_guard() = delete;
    explicit no_thread_lock_guard([[maybe_unused]] Mutex& mutex) noexcept{}
    no_thread_lock_guard(const no_thread_lock_guard&) = delete;
    no_thread_lock_guard& operator=(const no_thread_lock_guard&) = delete;
    no_thread_lock_guard(no_thread_lock_guard&&) = delete;
    no_thread_lock_guard& operator=(no_thread_lock_guard&&) = delete;
	~no_thread_lock_guard() noexcept = default;
};

template <typename Mutex>
class no_thread_unique_lock {
public:
    no_thread_unique_lock() noexcept = default;
    explicit no_thread_unique_lock([[maybe_unused]] Mutex& mutex) noexcept {}
    ~no_thread_unique_lock() noexcept {}
    no_thread_unique_lock(const no_thread_unique_lock&) = delete;
    no_thread_unique_lock& operator=(const no_thread_unique_lock&) = delete;
    no_thread_unique_lock([[maybe_unused]] no_thread_unique_lock&& other) noexcept {
    }

    no_thread_unique_lock& operator=([[maybe_unused]] no_thread_unique_lock&& other) noexcept {
        return *this;
    }

    void lock() noexcept {}

    bool try_lock() noexcept {
		return true;
    }

    void unlock() noexcept {}

    [[nodiscard]] bool owns_lock() const noexcept {
        return true;
    }

    Mutex* mutex() const noexcept {
        return nullptr;
    }

private:
    Mutex* mutex_ = nullptr;
    bool owns_lock_ = false;
};

template <typename Mutex>
using no_thread_shared_lock = no_thread_unique_lock<Mutex>;

namespace detail {

template <bool ThreadsAvailable>
struct configurable_mutex_selector {
    using type = no_thread_mutex;
};

template <bool ThreadsAvailable>
struct configurable_shared_mutex_selector {
    using type = no_thread_shared_mutex;
};

#if defined(NDOF_THREADS_FEATURE_ENABLED) && NDOF_THREADS_FEATURE_ENABLED == 1

template <bool ThreadsAvailable>
    requires (ThreadsAvailable && ::ndof::threads_feature_enabled())
struct configurable_mutex_selector<ThreadsAvailable> {
    using type = std::mutex;
};

template <bool ThreadsAvailable>
    requires (ThreadsAvailable && ::ndof::threads_feature_enabled())
struct configurable_shared_mutex_selector<ThreadsAvailable> {
    using type = std::shared_mutex;
};

#endif

} // namespace detail

template <bool ThreadsAvailable>
using configurable_mutex = typename detail::configurable_mutex_selector<ThreadsAvailable>::type;

template <bool ThreadsAvailable>
using configurable_shared_mutex =
    typename detail::configurable_shared_mutex_selector<ThreadsAvailable>::type;

#if defined(NDOF_THREADS_FEATURE_ENABLED) && NDOF_THREADS_FEATURE_ENABLED == 1

template <bool ThreadsAvailable>
using configurable_thread_guard = std::lock_guard<configurable_mutex<ThreadsAvailable>>;

template <bool ThreadsAvailable>
using configurable_unique_thread_guard = std::unique_lock<configurable_mutex<ThreadsAvailable>>;

template <bool ThreadsAvailable>
using configurable_shared_thread_guard =
    std::shared_lock<configurable_shared_mutex<ThreadsAvailable>>;

template <bool ThreadsAvailable>
using configurable_unique_shared_thread_guard =
    std::unique_lock<configurable_shared_mutex<ThreadsAvailable>>;

#else

template <bool ThreadsAvailable>
using configurable_thread_guard = no_thread_lock_guard<configurable_mutex<ThreadsAvailable>>;

template <bool ThreadsAvailable>
using configurable_unique_thread_guard =
    no_thread_unique_lock<configurable_mutex<ThreadsAvailable>>;

template <bool ThreadsAvailable>
using configurable_shared_thread_guard =
    no_thread_shared_lock<configurable_shared_mutex<ThreadsAvailable>>;

template <bool ThreadsAvailable>
using configurable_unique_shared_thread_guard =
    no_thread_unique_lock<configurable_shared_mutex<ThreadsAvailable>>;

#endif

template <bool ThreadsAvailable>
[[nodiscard]] configurable_thread_guard<ThreadsAvailable>
make_configurable_thread_guard(configurable_mutex<ThreadsAvailable>& mutex) {
    return configurable_thread_guard<ThreadsAvailable>{mutex};
}

template <bool ThreadsAvailable>
[[nodiscard]] configurable_unique_thread_guard<ThreadsAvailable>
make_configurable_unique_thread_guard(configurable_mutex<ThreadsAvailable>& mutex) {
    return configurable_unique_thread_guard<ThreadsAvailable>{mutex};
}

template <bool ThreadsAvailable>
[[nodiscard]] configurable_shared_thread_guard<ThreadsAvailable>
make_configurable_shared_thread_guard(configurable_shared_mutex<ThreadsAvailable>& mutex) {
    return configurable_shared_thread_guard<ThreadsAvailable>{mutex};
}

template <bool ThreadsAvailable>
[[nodiscard]] configurable_unique_shared_thread_guard<ThreadsAvailable>
make_configurable_unique_shared_thread_guard(
    configurable_shared_mutex<ThreadsAvailable>& mutex) {
    return configurable_unique_shared_thread_guard<ThreadsAvailable>{mutex};
}

} // namespace ndof

#endif // NDOF_CORE_THREAD_FEATURES_HPP
