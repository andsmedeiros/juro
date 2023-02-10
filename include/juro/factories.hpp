#ifndef JURO_FACTORIES_HPP
#define JURO_FACTORIES_HPP

#include "juro/helpers.hpp"

namespace juro::factories {
using namespace juro::helpers;

template<class T>
static void nop(promise_ptr<T> &) {  }

template<class T = void, class T_launcher = decltype(nop<T>)>
auto make_promise(T_launcher &&launcher = nop<T>) {
    static_assert(
        std::is_invocable_v<T_launcher, promise_ptr<T> &>,
        "Launcher function has an incompatible signature."
    );
    auto p = std::make_shared<promise<T>>();
    launcher(p);
    return p;
}

template<class T>
auto make_resolved(T &&value) {
    using bare_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return std::make_shared<promise<bare_type>>(
        resolved_promise_tag {  }, 
        std::forward<T>(value)
    );
}

inline auto make_resolved() { 
    return std::make_shared<promise<void>>(
        resolved_promise_tag {  }, 
        void_type {  }
    );
}

template<class T>
auto make_rejected(T &&value) {
    using bare_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return std::make_shared<promise<bare_type>>(
        rejected_promise_tag {  }, 
        std::forward<T>(value)
    );
}

inline auto make_rejected() {
    return std::make_shared<promise<void>>(
        rejected_promise_tag {  },
        promise_error { "Promise was rejected" }
    );
}

} /* namespace juro::factories */

#endif /* JURO_FACTORIES_HPP */