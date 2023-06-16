# Juro

> *Eu juro*
>
> *Por mim mesmo, por deus, por meus pais*
>
> *Vou te amar~~~*


**Juro** is a modern implementation of Javascript promises in C++17. It aims to 
provide a building block with which asynchronous agents can easily interact, while 
maintaining a readable and safe interface.  

## Table of contents

<!-- TOC -->
* [Juro](#juro)
  * [Table of contents](#table-of-contents)
  * [Disclaimer](#disclaimer)
  * [Basic example](#basic-example)
  * [Promises (and a little of Javascript)](#promises-and-a-little-of-javascript)
    * [An introduction to Javascript promises](#an-introduction-to-javascript-promises)
    * [Juro: an approximation of JS promises that leverages C++ facilities](#juro-an-approximation-of-js-promises-that-leverages-c-facilities)
      * [Event loop?](#event-loop)
    * [`juro::promise` and `juro::promise_ptr`](#juropromise-and-juropromiseptr)
    * [Promise factories](#promise-factories)
    * [Promise settling](#promise-settling)
      * [Handling resolution](#handling-resolution)
      * [Handling rejection](#handling-rejection)
      * [Handling resolution and rejection at once](#handling-resolution-and-rejection-at-once)
      * [Handling after settling](#handling-after-settling)
    * [Promise chaining](#promise-chaining)
  * [Roadmap](#roadmap)
<!-- TOC -->

## Disclaimer

This is a WIP. Tests are not comprehensive, documentation is lacking and the API is still being 
defined. It is however feature rich and very functional.

## Basic example

```C++
#include <juro/promise.hpp>

juro::make_promise<int>([] (auto &promise) {
    start_some_async_operation(promise);
})
->then([] (int value) {
    std::cout << "Got value" << value << std::endl;
});

// In `start_some_async_operation`, some time later:
promise->resolve(10);
```

## Promises (and a little of Javascript)

### An introduction to Javascript promises

(If you are comfortable with the concept of a JS Promise, feel free to skip this).

A promise of a value is an object that can be created even when that value is still not available
and can, once the value is available, hold and provide it to subsequent steps that depend on that 
value.

For example, in Javascript:

```JS
new Promise(resolve => setTimeout(() => resolve(327), 100))
```

A new `Promise` object is created (via `operator new`) and its constructor is invoked with a
function parameter. This function takes a `resolve` functor as parameter that can be used to
*settle* the promise as *resolved*. The function then proceeds to schedule the call of `resolve` 
with a value `327` 100 milliseconds into the future.

Now, suppose we didn't know with which value the promise would be resolved. Nonetheless, we could
still define what should be done with whichever value the promise would be resolved.

```JS
function asyncNumberGenerator(callback) {
    setTimeout(() => callback(Math.random()), 1000)
}

new Promise(resolve => asyncNumberGenerator(resolve))
    .then(number => console.log(`Got number: ${number}`))
```

The function `asyncNumberGenerator` invokes the provided `callback` function with a random number 
one second after called. Because it is provided the `resolve` functor, it will resolve the
promise with a random value.

The call to `.then()` sets a handler that gets called when the promise is resolved. Once a value 
is available, the handler will be invoked with the value as parameter. In this case, 
`'Got number: NUMBER'` will be printed, for whichever `NUMBER` is received from the asynchronous
number generator.

Besides coordinating asynchronous work, promises can also be used to set asynchronous error 
recovery paths. For example, the following function takes the path of a file as parameter and 
prints its content on the console:

```JS
import { readFile } from 'node:fs'

function printFileContents(path) {
    new Promise(resolve => readFile(path, (err, data) => resolve(data)))
        .then(contents => console.log(`Contents of file "${file}":`, contents))
}

printFileContents('path/to/file.txt')
```

If an invalid path is provided, the function fails to abort the process. Instead, it will output
the same as if the file existed but was empty. 
To clearly indicate that an error occurred and offer possibility of error handling, the promise 
can be *rejected*:

```JS
// ...
new Promise((resolve, reject) => readFile(path, (err, data) => {
    if(err) { reject(err) }
    else { resolve(data) }
}))
// ...
```

Now, when an invalid path is passed to out function, an error will be thrown. To handle it, a
rejection handler can be attached:

```JS
// ...
    .then(
        contents => console.log(`Contents of file "${file}":`, contents),
        error => console.error(`Failed to open file "${file}":`, error)
    )
// ...
```

This way, the error can be caught and a suitable error message will be printed to `stderr`.

At last, promises can be chained. All calls to `.then()` (and its cousin functions `.catch()` 
and `.finally()`) return another promise that can also be `.then`ed.

```JS
function resolvesInOneSecond() {
    return new Promise(resolve => setTimeout(resolve, 1000))
}

resolvesInOneSecond()
    .then(() => {
        console.log("A")
        return resolvesInOneSecond()
    })
    .then(() => {
        console.log("B")
        return resolvesInOneSecond()
    })
    .then(() => {
        console.log("C")
    })
```

As each call to `resolvesInOneSecond()` returns a new promise, each handler attached by `.then()`
will only be called when the previous promise is settled. As such, `ABC` is printed with a 
one-second-delay between each character.

**See also:** [MDN documentation on promises](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)

### Juro: an approximation of JS promises that leverages C++ facilities

C++ and Javascript are very different languages. Many features that make implementing promises
in Javascript very easy, such as weak typing, dynamic objects with counted references by default,
lack of multithreading etc. are not as immediately available to C++ programs or do not live up to
design, performance and safety expectations of C++ developers.

Juro attempts to bridge these two words. It provides a way to represent asynchronous operations
as self-contained objects that can be composed irrespective of time, while also leveraging C++ 
idioms, STL containers and compile-time type safety.

Juro's API is inspired and modelled after Javascript's one.

#### Event loop?

Javascript is a single threaded environment that operates around an event loop. Promises are,
therefore, guaranteed to be scheduled and ran on the same single thread. This is not true for
C++ programs, at least not necessarily. 

To avoid concurrency issues, this guide will presume there is available an event loop object
with capabilities and API similar to JS's one and that every instruction run will originate in 
it:

```C++
#include <functional>

class event_loop {
public:
    using task = std::function<void()>;
    void schedule(task, unsigned int delay = 0);
    
    // other implementation details
};

event_loop loop;
```

### `juro::promise` and `juro::promise_ptr`

All promises in Juro are represented by instances of the `juro::promise<T>` template. However,
because asynchronous operations must be accessible at a later time for settling, they are
dynamically allocated and managed by shared pointers -- `std::shared_ptr<juro::promise<T>>`.

To avoid the cumbersome template instantiating, there is a conveniency alias 
`juro::promise_ptr<T>` that should be preferred and that will be used throughout this guide.

### Promise factories

All promises are meant to be created via one of the available promise factories.

The most immediately obvious factory mimics Javascript's promise constructor:

```C++
template<class T, class T_launcher>
juro::promise_ptr<T> juro::make_promise(T_launcher &&launcher);
```

`juro::make_promise` takes a single launcher functor argument, that gets a fresh 
`const promise_ptr<T> &` as parameter. This handle allows for an asynchronous operation to settle
the promise later on. A copy of the handle is also returned:

```C++
juro::promise_ptr<std::string> promise =
    juro::make_promise<std::string>([] (const juro::promise_ptr<std::string> &promise) {
        store_handle_and_settle_later_on(promise);
    });

// or the much more readable form:
auto promise = juro::make_promise<std::string>([] (auto &promise) {
   store_handle_and_settle_later_on(promise); 
});
```
Promises can also be explicitly created in any state:

```C++
// juro::promise_ptr<bool>
auto pending_promise = juro::make_pending<bool>();

// juro::promise_ptr<int>
auto resolved_promise = juro::make_resolved<int>(); 

// juro::promise_ptr<std::string>
auto rejected_promise = 
    juro::make_rejected<std::string>(std::runtime_error { "Runtime error" });
```

When the template parameter is excluded from any factory function, they default to creating 
`juro::promise<void>`.

### Promise settling

Unlike Javascript promises that can only be settled through the `resolve` and `reject` functors
provided by the promise constructor, `juro::promise`s can be settled by any party that owns a
copy of its `juro::promise_ptr`.

```C++
auto promise = juro::make_pending();
loop.schedule([promise] { promise->resolve(); }, 1000);
```

Once a promise is settled, it retains its state and value forever; attempting to resettle it 
will trigger an exception.

#### Handling resolution

To handle a resolved value, a settle handler can be installed via `.then()`:

```C++
juro::make_promise<int>([] (auto &promise) {
    loop.schedule([promise] { promise->resolve(100); }, 1000);
})
->then([] (int value) {
    std::cout << "Got value " << value << std::endl;
});
```

If a promise is of type void (`juro::promise<void>`), its resolve handler must not take any
parameter.
```C++
juro::make_promise<void>([] (auto &promise) {
    loop.schedule([promise] { promise->resolve(); }, 1000);
})
->then([] {
    std::cout << "Promise resolved!" << std::endl;
});
```

The functor passed to `.then()` is statically checked, so there is no risk of type mismatch 
between resolution and handling. Also, no type erasing or dynamic allocation is needed to store 
the value; it lives right in the `juro::promise`. There is only a single `std::function` type 
erased object involved in the promise chain; once the chain is constructed, no more memory is
necessary.

#### Handling rejection

The semantics of promise rejection in Javascript imply that any exception thrown inside an
asynchronous task gets translated as the rejection of the promise that represents that
asynchronous task.

Because Juro implements this behaviour, the rejection handling API is designed considering C++'s
exception system: anything thrown is type-erased through a `std::exception_ptr`, and so is the
type of the parameter taken in rejection handling functors.

The most obvious way to attach such a handler is through `.rescue()`:

```C++
#include <string>

using namespace std::string_literals;

juro::make_promise([] (auto &promise) {
    loop.schedule([promise] { promise->reject("Here lies an error"s); }, 1000);
})
// std::exception_ptr is a type-erased pointer
->rescue([] (std::exception_ptr err) { 
   try {
       // by rethrowing the exception pointer we can disambiguate
       std::rethrow_exception(err); 
       
   // a std::string was thrown, we know how to catch it
   } catch(const std::string &message) { 
       std::cout << "Error: " << message << std::endl;
   
   // some other thing we don't know was thrown
   } catch(...) {
       std::cout << "Unknown error" << std::endl;
   }
});

// After one second, will print "Error: Here lies an error".
```

#### Handling resolution and rejection at once

`.then()` can be used to attach two mutually-exclusive handler at once, each one fit for one
settled state. This is useful to split execution path into two:

```C++
#include <ctime>
#include <cstdlib>

std::srand(std::time(nullptr));

juro::make_promise<int>([] (auto &promise) {
    loop.schedule([promise] {
        auto value = std::rand(); // get a random number
        if(value % 2) {
            promise->resolve(value); // resolves if it's even
        } else {
            promise->reject(value); // rejects if it's odd
        }
    }, 1000);
})
->then( 
    // As the promise cannot be both resolved and rejected, only one of these handlers will be
    // invoked: the first handles resolution and the latter handles rejection
    [] (int value) { 
        std::cout << "Promise was resolved with " << value << std::end; 
    },
    [] (std::exception_ptr err) {
        try {
            std::rethrow_exception(err);
        } catch(int value) {
            std::cout << "Promise was rejected with" << value << std::endl;
        }
    }
);
```

This is the most generalised form of promise handling; `.then(on_resolve)`, `.rescue(on_reject)` 
and `.finally(on_settle)` are all defined in terms of `.then(on_resolve, on_reject)`.

`.finally()` attaches the same handler for both states, so the provided handler must be able to 
identify and access both a promised value, in case of resolution, or an `std::exception_ptr`, in
case of rejection. The type of the handler parameter is a little bit complicated and will be
clarified later on.

#### Handling after settling

As is with Javascript promises, resolving a promise with a value causes the value to be stored
inside the promise object. If a settle handler is installed by then it is called immediately,
otherwise, it is called when installed. Rejecting a promise when no handler is attached results
in an exception that propagates up to the point of rejection.

```C++
auto p1 = juro::make_promise<std::string>([] (auto &promise) {
    promise->resolve("Resolved"s); // Ok, "Resolved"s gets stored inside *p1
});

p1->then([] (std::string &value) {  
    std::cout << "Resolved with " << value << std::endl;
}); // OK, gets called immediately because the promise is already resolved

auto p2 = juro::make_promise([] (auto &promise) {
    promise->reject(); // Not OK, throws juro::promise_error { "Unhandled promise rejection" } 
});

p2->rescue([] (std::exception_ptr err) {
        // ...
}); // Never gets called because an exception has been thrown sooner
```

There is one special case in which `juro::promise`s are different from JS promises: a rejected 
promise can be safely created with `juro::make_rejected()` and no exception will be thrown.
A handler can still be attached later, and when it does, its rejection functor will be invoked.

```C++
auto promise = std::make_rejected("Rejected here"s);
promise->rescue([] (std::exception_ptr err) {
   try { std::rethrow_exception(err); }
   catch(std::string &message) { std::cout << message << std::endl; }
}); // OK, gets invoked immediately
```

### Promise chaining

TODO

## Roadmap
- Comprehensive test suite
- Comprehensive documentation
- Fill in some API gaps (`.allSettled()` and so on)
- Enhance `juro::promise_ptr` with conveniency aliases