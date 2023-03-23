# Juro

> *Eu juro*
>
> *Por mim mesmo, por Zeus, por meus pais*
>
> *Vou te amar~~~*


**Juro** is a modern implementation of Javascript promises in C++17. It aims to 
provide a building block with which asynchronous agents can interact.

## Basic example

```C++
#include <juro/promise.hpp>

juro::make_promise<int>([] (auto &promise) {
    start_some_async_operation(promise);
})->then([] (int value) {
    std::cout << "Got value" << value << std::endl;
});

// In `start_some_async_operation`, some time later:
promise->resolve(10);

```