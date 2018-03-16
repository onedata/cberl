/**
 * @file ResponsePlaceholder.h
 * @author Bartek Kryza
 * @copyright (C) 2018
 * This software is released under the MIT license cited in 'LICENSE.md'
 */
#ifndef COUCHBASE_RESPONSE_PLACEHOLDER_H
#define COUCHBASE_RESPONSE_PLACEHOLDER_H

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace cb {

/**
 * @c ResponsePlaceholder provides interface for storing cberl Responses
 * until they are completed by asynchronous responses from Couchbase
 * server.
 */
template <class TRes> class ResponsePlaceholder {
public:
    void forgetResponse(uint64_t id);

    void emitResponse(uint64_t id);

    uint64_t storeResponse(
        TRes &&value, std::function<void(const TRes &)> callback);

    void storeResponse(
        uint64_t id, TRes &&value, std::function<void(const TRes &)> callback);

    TRes &getResponse(uint64_t id);

private:
    std::unordered_map<uint64_t,
        std::tuple<TRes, std::function<void(const TRes &)>>>
        m_responses;

    uint64_t m_responseNextId{0};

    std::mutex m_mutex;
};

template <class TRes>
uint64_t ResponsePlaceholder<TRes>::storeResponse(
    TRes &&value, std::function<void(const TRes &)> callback)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    assert(callback);

    uint64_t id = m_responseNextId++;

    assert(m_responses.find(id) == m_responses.end());

    m_responses.emplace(id,
        std::make_tuple<TRes, std::function<void(const TRes &)>>(
            std::move(value), std::move(callback)));
    return id;
}

template <class TRes>
void ResponsePlaceholder<TRes>::storeResponse(
    uint64_t id, TRes &&value, std::function<void(const TRes &)> callback)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    assert(callback);

    assert(m_responses.find(id) == m_responses.end());

    m_responses.emplace(id,
        std::make_tuple<TRes, std::function<void(const TRes &)>>(
            std::move(value), std::move(callback)));
}

template <class TRes> TRes &ResponsePlaceholder<TRes>::getResponse(uint64_t id)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    assert(m_responses.find(id) != m_responses.end());

    return std::get<0>(m_responses[id]);
}

template <class TRes>
void ResponsePlaceholder<TRes>::forgetResponse(uint64_t id)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    m_responses.erase(id);
}

template <class TRes> void ResponsePlaceholder<TRes>::emitResponse(uint64_t id)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    assert(m_responses.find(id) != m_responses.end());

    auto response = std::get<0>(m_responses[id]);
    auto callback = std::get<1>(m_responses[id]);

    assert(callback);

    if(callback)
        callback(response);
}
}

#endif // COUCHBASE_RESPONSE_PLACEHOLDER_H
