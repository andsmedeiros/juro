#ifndef JURO_PROMISE_HPP
#define JURO_PROMISE_HPP

#include<functional>
#include <memory>
#include <stdexcept>
#include <variant>
#include "juro/helpers.hpp"
#include "juro/factories.hpp"
#include "juro/compose/all.hpp"

namespace juro {

using namespace juro::helpers;
using namespace juro::factories;
using namespace juro::compose;

/**
 * @brief A promise represents a value that is not available yet.
 * @tparam T The type of the promised value
 */
template<class T = void>
class promise final {
    template<class> friend class promise;
public:

    /**
     * @brief True for void promises, false otherwise
     */
    static constexpr inline bool is_void = std::is_void_v<T>;

    /**
     * @brief Maps `void` to `void_type`, every other type maps to itself
     */
    using value_type = storage_type<T>;

    /**
     * @brief Represents the possible values a promise can hold: a pending
     * promise holds an `empty_type`, a resolved promise holds a `value_type` 
     * and a rejected promise holds an `std::exception_ptr`.
     */
    using settle_type = 
        std::variant<empty_type, value_type, std::exception_ptr>;

private:
    /**
     * @brief Holds the current state of the promise; Once settled, it cannot be
     * changed.
     */
    promise_state state = promise_state::PENDING;

    /**
     * @brief Holds the settled value or `empty_type` if the promise is pending.
     */
    settle_type value;

    /**
     * @brief Type-erased callback to be executed once the promise is settled.
     */
    std::function<void()> on_settle;

public:

    /**
     * @brief The promised object type.
     */
    using type = T;

    /**
     * @brief Constructs a pending promise.
     * @warning This should not be called directly; use `juro::make_promise()` 
     * instead.
     */
    promise() = default;

    /**
     * @brief Constructs a settled promise.
     * @warning This should not be called directly; use `juro::make_resolved()` 
     * or `juro::make_rejected()` instead.
     * @tparam T_value The settled value for this promise.
     * @param state The state as the promise should be settled.
     * @param value The value the promise should hold. When rejected, if it is
     * not an `std::exception_ptr`, it will be stored into one.
     */
    template<class T_value>
    promise(resolved_promise_tag, T_value &&value) : 
        state { promise_state::RESOLVED },
        value { std::forward<T_value>(value) }
        {  }

    template<class T_value>
    promise(rejected_promise_tag, T_value &&value) : 
        state { promise_state::REJECTED },
        value { rejection_value(std::forward<T_value>(value)) }
        {  }

    promise(promise &&) = delete;
    promise(const promise &) = delete;
    ~promise() noexcept = default;

    promise &operator=(promise &&) = delete;
    promise &operator=(const promise &) = delete;

    inline promise_state get_state() const noexcept { return state; }
    
    inline bool is_pending() const noexcept { 
        return state == promise_state::PENDING; 
    }
    
    inline bool is_resolved() const noexcept { 
        return state == promise_state::RESOLVED; 
    }
    
    inline bool is_rejected() const noexcept { 
        return state == promise_state::REJECTED; 
    }
    
    inline bool is_settled() const noexcept { 
        return state != promise_state::PENDING; 
    }

    template<class T_value>
    inline bool holds_value() const noexcept { 
        return std::holds_alternative<T_value>(value); 
    }

    inline bool is_empty() const noexcept {
        return std::holds_alternative<empty_type>(value);
    }

    inline bool has_handler() const noexcept { 
        return static_cast<bool>(on_settle); 
    }

    value_type &get_value() {
        return std::get<value_type>(value);
    }

    std::exception_ptr &get_error() {
        return std::get<std::exception_ptr>(value);
    }

    /**
     * @brief Resolves the promise with a given value. Fires the settle handler 
     * if there is one attached.
     * @tparam T_value The value type with which to settle the promise. Must be
     * convertible to `T`.
     * @param resolved_value The value with which to settle the promise.
     */
    template<class T_value = value_type>
    void resolve(T_value &&resolved_value = {}) {
        static_assert(
            std::is_convertible_v<T_value, value_type>, 
            "Resolved value is not convertible to promise type"
        );

        if(state != promise_state::PENDING) {
            throw promise_error { "Attempted to resolve settled promise" };
        }

        state = promise_state::RESOLVED;
        value = std::forward<T_value>(resolved_value);

        if(on_settle) {
            on_settle();
        }
    }

    /**
     * @brief Rejects the promise with a given value. Fires the settle handler 
     * if there is one attached, otherwise throws a `juro::promise_exception`.
     * @tparam T_value The value type with which to settle the promise.
     * @param rejected_value The value with which to settle the promise. If it 
     * is not an `std::exception_ptr`, it will be stored into one.
     */
    template<class T_value = promise_error>
    void reject(T_value &&rejected_value = 
        promise_error { "Promise was rejected" }
    ) {
        if(state != promise_state::PENDING) {
            throw promise_error { "Attempted to reject settled promise" };
        }

        state = promise_state::REJECTED;
        value = rejection_value(std::forward<T_value>(rejected_value));

        if(on_settle) {
            on_settle();
        } else {
            throw promise_error { "Unhandled promise rejection" }; 
        }
    }

    /**
     * @brief Attaches a settle handler to the promise, overwriting any
     * previously attached one.
     * @tparam T_on_resolve The type of the resolve handler; should receive the 
     * promised type as parameter, preferably as a reference.
     * @tparam T_on_reject The type of the reject handler; should receive an
     * `std::exception_ptr` as parameter, preferably as a reference.
     * @param on_resolve The functor to be invoked when the promise is resolved.
     * @param on_reject The functor to be invoked when the promise is rejected.
     * @return A new promise of a type that depends on the types returned by the
     * functors provided.
     * @see `juro::type_helpers::chained_promise_type`
     */
    template<class T_on_resolve, class T_on_reject>
    auto then(T_on_resolve &&on_resolve, T_on_reject &&on_reject) {
        assert_resolve_invocable<T_on_resolve>();
        assert_reject_invocable<T_on_reject>();
        
        using next_value_type = 
            chained_promise_type<T, T_on_resolve, T_on_reject>;

        return make_promise<next_value_type>([&] (auto &next_promise) {
            on_settle = [
                this,
                next_promise,
                on_resolve = std::forward<T_on_resolve>(on_resolve),
                on_reject = std::forward<T_on_reject>(on_reject)
            ] {
                try {
                    switch (state) {
                    case promise_state::RESOLVED:  
                        handle_resolve(on_resolve, next_promise);
                        break;
                    
                    case promise_state::REJECTED:
                        handle_reject(on_reject, next_promise);
                        break;
                    default: break;
                    }
                } catch(...) {
                    next_promise->reject(std::current_exception());
                }
            };
            if(state != promise_state::PENDING) {
                on_settle();
            }
        });
    }

    /**
     * @brief Attaches a resolve handler to the promise, overwriting any 
     * previously attached one. In case of rejection, the error will be 
     * propagated.
     * @tparam T_on_resolve The type of the resolve handler; should receive the
     * promise type as parameter, preferably by reference.
     * @param on_resolve The functor to be invoked when the promise is resolved.
     * @return 
     */
    template<class T_on_resolve>
    inline auto then(T_on_resolve &&on_resolve) {
        return then(std::forward<T_on_resolve>(on_resolve), [] (auto &error) {
            std::rethrow_exception(error);
        });
    }

    template<class T_on_reject>
    inline auto rescue(T_on_reject &&on_reject) {
        if constexpr(is_void) {
            return then([] {}, std::forward<T_on_reject>(on_reject));
        } else {
            return then(
                [] (auto &value) { return value; },
                std::forward<T_on_reject>(on_reject)
            );
        }
    }

    template<class T_on_settle>
    inline auto finally(T_on_settle &&on_settle) {
        assert_settle_invocable<T_on_settle>();

        using next_value_type = 
            chained_promise_type<T, T_on_settle, T_on_settle>;

        return make_promise<next_value_type>([&] (auto &next_promise) {
            this->on_settle = [
                this,
                next_promise,
                on_settle = std::forward<T_on_settle>(on_settle)
            ] {
                try {
                    switch (state) {
                    case promise_state::RESOLVED:  
                        handle_resolve(on_settle, next_promise);
                        break;
                    
                    case promise_state::REJECTED:
                        handle_reject(on_settle, next_promise);
                        break;
                    default: break;
                    }
                } catch(...) {
                    next_promise->reject(std::current_exception());
                }
            };
            if(state != promise_state::PENDING) {
                this->on_settle();
            }
        });
    }

private:
    template<class T_on_resolve>
    static inline void assert_resolve_invocable() noexcept {
        static_assert(
            (is_void && std::is_invocable_v<T_on_resolve>) ||
            std::is_invocable_v<T_on_resolve, value_type &>,
            "Resolve handler has an incompatible signature."
        );
    }

    template<class T_on_reject>
    static inline void assert_reject_invocable() noexcept {
        static_assert(
            std::is_invocable_v<T_on_reject, std::exception_ptr &>,
            "Reject handler has an incompatible signature."
        );
    }

    template<class T_on_settle>
    static inline void assert_settle_invocable() noexcept {
        static_assert(
            std::is_invocable_v<T_on_settle, finally_argument_t<T> &>,
            "Settle handler has an incompatible signature."
        );
    }

    template<class T_on_resolve, class T_next_promise>
    std::enable_if_t<is_void && resolves_void_v<T, T_on_resolve>>
    handle_resolve(T_on_resolve &on_resolve, T_next_promise &next_promise) {
        on_resolve();
        next_promise->resolve();
    }

    template<class T_on_resolve, class T_next_promise>
    std::enable_if_t<is_void && resolves_value_v<T, T_on_resolve>>
    handle_resolve(T_on_resolve &on_resolve, T_next_promise &next_promise) {
        next_promise->resolve(on_resolve());
    }

    template<class T_on_resolve, class T_next_promise>
    std::enable_if_t<is_void && resolves_promise_v<T, T_on_resolve>>
    handle_resolve(T_on_resolve &on_resolve, T_next_promise &next_promise) {
        on_resolve()->pipe(next_promise);
    } 

    template<class T_on_resolve, class T_next_promise>
    std::enable_if_t<!is_void && resolves_void_v<T, T_on_resolve>>
    handle_resolve(T_on_resolve &on_resolve, T_next_promise &next_promise) {
        on_resolve(std::get<T>(value));
        next_promise->resolve();
    }

    template<class T_on_resolve, class T_next_promise>
    std::enable_if_t<!is_void && resolves_value_v<T, T_on_resolve>>
    handle_resolve(T_on_resolve &on_resolve, T_next_promise &next_promise) {
        next_promise->resolve(on_resolve(std::get<T>(value)));
    }

    template<class T_on_resolve, class T_next_promise>
    std::enable_if_t<!is_void && resolves_promise_v<T, T_on_resolve>>
    handle_resolve(T_on_resolve &on_resolve, T_next_promise &next_promise) {
        on_resolve(std::get<T>(value))->pipe(next_promise);
    }

    template<class T_on_reject, class T_next_promise>
    std::enable_if_t<rejects_void_v<T_on_reject>>
    handle_reject(T_on_reject &&on_reject, T_next_promise &next_promise) {
        on_reject(std::get<std::exception_ptr>(value));
        next_promise->resolve();
    }

    template<class T_on_reject, class T_next_promise>
    std::enable_if_t<rejects_value_v<T_on_reject>>
    handle_reject(T_on_reject &&on_reject, T_next_promise &next_promise) {
        auto &rejected_value = std::get<std::exception_ptr>(value);
        next_promise->resolve(on_reject(rejected_value));
    }

    template<class T_on_reject, class T_next_promise>
    std::enable_if_t<rejects_promise_v<T_on_reject>>
    handle_reject(T_on_reject &&on_reject, T_next_promise &next_promise) {
        auto &rejected_value = std::get<std::exception_ptr>(value);
        on_reject(rejected_value)->pipe(next_promise);
    }

    template<class T_value>
    std::exception_ptr rejection_value(T_value &&value) {
        using bare_type = std::remove_cv_t<std::remove_reference_t<T_value>>;
        if constexpr(std::is_same_v<bare_type, std::exception_ptr>) {
            return std::forward<T_value>(value);
        } else {
            return std::make_exception_ptr(std::forward<T_value>(value));
        }
    }
    
    template<class T_next_promise>
    inline void pipe(T_next_promise &&next_promise) {
        if constexpr(is_void) {
            then(
                [=] { next_promise->resolve(); },
                [=] (auto &error) { next_promise->reject(error); }
            );
        } else {
            then(
                [=] (auto &value) { next_promise->resolve(value); },
                [=] (auto &error) { next_promise->reject(error); }
            );
        }
    }
};

} /* namespace juro */

#endif /* JURO_PROMISE_HPP */