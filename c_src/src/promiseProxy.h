/**
 * @file promiseProxy.h
 * @author Bartek Kryza
 * @copyright (C) 2018
 * This software is released under the MIT license cited in 'LICENSE.md'
 */
#ifndef COUCHBASE_PROMISE_PROXY_H
#define COUCHBASE_PROMISE_PROXY_H

#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace cb {

/**
 * @c PromiseProxy provides generic proxy for holding promises for futures,
 * which need to be maintained for some time (for instance in case of interface
 * with asynchronous callback-based API's) until the promise is fullfilled.
 * The promises can be identified based on ID.
 */
template <class PromiseType>
class PromiseProxy {
public:
    void fullfillPromise(uint64_t id);

    void cancelPromise(uint64_t id, std::runtime_error &&error);

    uint64_t addPromise(
        std::promise<PromiseType> &&promise, PromiseType &&value);

    PromiseType &promisedValue(uint64_t id);

private:
    std::unordered_map<uint64_t,
        std::tuple<std::promise<PromiseType>, PromiseType>>
        m_promises;
    uint64_t m_promiseNextId{0};
    std::mutex m_promiseMutex;
};

template <class PromiseType>
void PromiseProxy<PromiseType>::fullfillPromise(uint64_t id)
{
    std::lock_guard<std::mutex> guard(m_promiseMutex);
    auto &pt = m_promises[id];
    std::get<0>(pt).set_value(std::move(std::get<1>(pt)));
    m_promises.erase(id);
}

template <class PromiseType>
uint64_t PromiseProxy<PromiseType>::addPromise(
    std::promise<PromiseType> &&promise, PromiseType &&value)
{
    std::lock_guard<std::mutex> guard(m_promiseMutex);
    uint64_t promiseId = m_promiseNextId++;
    m_promises.emplace(
        promiseId, std::make_tuple(std::move(promise), std::move(value)));
    return promiseId;
}

template <class PromiseType>
PromiseType &PromiseProxy<PromiseType>::promisedValue(uint64_t id)
{
    std::lock_guard<std::mutex> guard(m_promiseMutex);
    auto &pt = m_promises[id];
    return std::get<1>(pt);
}

template <class PromiseType>
void PromiseProxy<PromiseType>::cancelPromise(
    uint64_t id, std::runtime_error &&error)
{
    std::lock_guard<std::mutex> guard(m_promiseMutex);
    auto &pt = m_promises[id];
    std::get<0>(pt).set_exception(std::make_exception_ptr(std::move(error)));
    m_promises.erase(id);
}
}

#endif // COUCHBASE_PROMISE_PROXY_H
