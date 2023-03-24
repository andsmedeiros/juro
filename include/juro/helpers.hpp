#ifndef JURO_HELPERS_HPP
#define JURO_HELPERS_HPP

#include <memory>
#include <optional>
#include <type_traits>
#include <stdexcept>
#include <variant>

namespace juro {
template<class> class promise;
} /* namespace juro */

namespace juro::helpers {

/**
 * @brief A shared pointer to a `promise<T>`
 * @tparam T The type of the promised value
 */
template<class T>
using promise_ptr = std::shared_ptr<promise<T>>;

/**
 * @brief The exception error thrown by invalid promise operations
 */
struct promise_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/**
 * @brief The possible states of a promise at one time
 */
enum class promise_state { PENDING, RESOLVED, REJECTED };

struct resolved_promise_tag {  };
struct rejected_promise_tag {  };

/**
 * @brief Tag struct to represent an absent value
 */
struct empty_type {  };

/**
 * @brief Tag struct to represent a void value
 */
struct void_type {  
    friend inline bool operator==(const void_type &, const void_type &) noexcept {
        return true; 
    }
};

template<class T>
using storage_type = std::conditional_t<std::is_void_v<T>, void_type, T>;

/**
 * @brief Defines a container suitable to hold two different types. 
 * @details This clause activates when both types are diferent. The internal 
 * `type` is a `std::variant<T1, T2>`.
 * @tparam T1 
 * @tparam T2
 */
template<class T1, class T2, class = void>
struct common_container {
    using type = std::variant<T1, T2>;
};

/**
 * @brief Defines a container suitable to hold two different types. 
 * @details This clause activates when the first type is `void`. The internal 
 * `type` is a `std::optional<T>`.
 * @tparam T The non-void type
 */
template<class T>
struct common_container<void, T> {  
    using type = std::optional<T>;
};

/**
 * @brief Defines a container suitable to hold two different types. 
 * @details This clause activates when the second type is `void`. The internal 
 * `type` is a `std::optional<T>`.
 * @tparam T The non-void type
 */
template<class T>
struct common_container<T, void> {  
    using type = std::optional<T>;
};

/**
 * @brief Defines a container suitable to hold two different types. 
 * @details This clause activates both types are `void`. The internal `type` 
 * is a `void`.
 * @tparam T The non-void type
 */
template<>
struct common_container<void, void> {  
    using type = void;
};

/**
 * @brief Defines a container suitable to hold two different types. 
 * @details This clause activates when both types are the same. The internal 
 * `type` is a `T`.
 * @tparam T The type supplied to both parameters
 */
template<class T>
struct common_container<T, T> {
    using type = T;
};

/**
 * @brief Helper type that aliases `common_container<T1, T2>::type`.
 * @tparam T1 
 * @tparam T2 
 */
template<class T1, class T2>
using common_container_t = typename common_container<T1, T2>::type;

template<class T>
struct unwrap_if_promise {
    using type = T;
};

template<class T>
struct unwrap_if_promise<promise_ptr<T>> {
    using type = typename unwrap_if_promise<T>::type;
};

template<class T>
using unwrap_if_promise_t = typename unwrap_if_promise<T>::type;

template<class T>
struct is_promise : public std::false_type { };

template<class T>
struct is_promise<promise_ptr<T>> : public std::true_type {  };

template<class T>
static constexpr inline bool is_promise_v = is_promise<T>::value;

template<class T, class T_on_resolve>
struct resolve_result {
    using type = std::invoke_result_t<T_on_resolve, T &>;
};

template<class T_on_resolve>
struct resolve_result<void, T_on_resolve> {
    using type = std::invoke_result_t<T_on_resolve>;
};

template<class T, class T_on_resolve>
using resolve_result_t = typename resolve_result<T, T_on_resolve>::type;

template<class T_on_reject>
using reject_result_t = std::invoke_result_t<T_on_reject, std::exception_ptr &>;

template<class T, class T_on_resolve, class T_on_reject>
using chained_promise_type = common_container_t<
    unwrap_if_promise_t<resolve_result_t<T, T_on_resolve>>,
    unwrap_if_promise_t<reject_result_t<T_on_reject>>
>;

template<class T>
using finally_argument_t =
    common_container_t<unwrap_if_promise_t<T>, std::exception_ptr>;

template<class T, class T_on_resolve>
static constexpr inline bool resolves_void_v = 
    std::is_void_v<resolve_result_t<T, T_on_resolve>>;

template<class T, class T_on_resolve>
static constexpr inline bool resolves_promise_v = 
    !std::is_void_v<resolve_result_t<T, T_on_resolve>> &&
    is_promise_v<resolve_result_t<T, T_on_resolve>>;

template<class T, class T_on_resolve>
static constexpr inline bool resolves_value_v = 
    !std::is_void_v<resolve_result_t<T, T_on_resolve>> &&
    !is_promise_v<resolve_result_t<T, T_on_resolve>>;

template<class T_on_reject>
static constexpr inline bool rejects_void_v = 
    std::is_void_v<reject_result_t<T_on_reject>>;

template<class T_on_reject>
static constexpr inline bool rejects_promise_v = 
    !std::is_void_v<reject_result_t<T_on_reject>> &&
    is_promise_v<reject_result_t<T_on_reject>>;

template<class T_on_reject>
static constexpr inline bool rejects_value_v = 
    !std::is_void_v<reject_result_t<T_on_reject>> &&
    !is_promise_v<reject_result_t<T_on_reject>>;
    
template<class T, class T_on_resolve>
using resolved_promise_value_t = 
    unwrap_if_promise_t<resolve_result_t<T, T_on_resolve>>;
    
template<class T_on_resolve>
using rejected_promise_value_t = 
    unwrap_if_promise_t<reject_result_t<T_on_resolve>>;

template<class T_tuple, class ...>
struct unique { 
    using type = T_tuple; 
};

template<class ...T_values, class T, class ...T_rest>
struct unique<std::tuple<T_values...>, T, T_rest...> {
    using type = std::conditional_t<
        std::disjunction_v<std::is_same<T, T_values>...>,
        typename unique<std::tuple<T_values...>, T_rest...>::type,
        typename unique<std::tuple<T_values..., T>, T_rest...>::type
    >;
};

template<class ...T_values>
struct unique<std::tuple<>, std::tuple<T_values...>> : 
    unique<std::tuple<>, T_values...> {  };

template<class ...T_values>
using unique_t = typename unique<std::tuple<>, std::tuple<T_values...>>::type;

template<class T>
using bare_t = std::remove_reference_t<std::remove_cv_t<T>>;

} /* namespace juro::helpers */

#endif /* JURO_HELPERS_HPP */