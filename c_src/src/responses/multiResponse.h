/**
 * @file multiResponse.h
 * @author Krzysztof Trzepla
 * @copyright (C) 2017: Krzysztof Trzepla
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

#ifndef CBERL_MULTI_RESPONSE_H
#define CBERL_MULTI_RESPONSE_H

#include "response.h"

#include <vector>

namespace cb {

template <class ResponseT> class MultiResponse : public Response {
public:
    MultiResponse(lcb_error_t err = LCB_SUCCESS, uint64_t batchSize = 1)
        : Response{err}
        , m_batchSize{batchSize}
    {
    }

    void add(ResponseT response)
    {
        m_responses.emplace_back(std::move(response));
    }

    bool complete() { return m_responses.size() == m_batchSize; }

#if !defined(NO_ERLANG)
    nifpp::TERM toTerm(const Env &env) const
    {
        if (m_err == LCB_SUCCESS) {
            std::vector<nifpp::TERM> terms;
            for (const auto &response : m_responses) {
                terms.emplace_back(response.toTerm(env));
            }
            return nifpp::make(
                env, std::make_tuple(nifpp::str_atom{"ok"}, std::move(terms)));
        }

        return Response::toTerm(env);
    }
#endif

private:
    std::vector<ResponseT> m_responses;
    uint64_t m_batchSize;
};

} // namespace cb

#endif // CBERL_MULTI_RESPONSE_H
