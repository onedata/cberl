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

    /**
     * Remove the response from the cache if exists.
     */
    void forgetResponse(uint64_t id);

    /**
     * Execute the callback registered with the response. The response
     * is not automatically removed from the cache after the callback
     * is executed.
     */
    void emitResponse(uint64_t id);

    /**
     * Add response to the cache. If a response with the same id
     * is already in the cache it will be replace with the new value.
     * The response will have an automatically assigned unique id.
     */
    uint64_t storeResponse(
        TRes &&value, std::function<void(const TRes &)> callback);

    /**
     * Add a response to the cache with a custom id. The user must
     * ensure it is unique.
     */
    void storeResponse(
        uint64_t id, TRes &&value, std::function<void(const TRes &)> callback);

    /**
     * Return the response for given id.
     */
    TRes &getResponse(uint64_t id);

    /**
     * Check if the cache has a response with given id.
     */
    bool hasResponse(uint64_t id);

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

template <class TRes> bool ResponsePlaceholder<TRes>::hasResponse(uint64_t id)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    return m_responses.find(id) != m_responses.end();
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

    if (callback)
        callback(response);
}
}

#endif // COUCHBASE_RESPONSE_PLACEHOLDER_H
