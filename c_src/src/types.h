/**
 * @file types.h
 * @author Krzysztof Trzepla
 * @copyright (C) 2017: Krzysztof Trzepla
 * This software is released under the MIT license cited in 'LICENSE.md'
 */

#ifndef CBERL_TYPES_H
#define CBERL_TYPES_H

#include <functional>
#include <memory>

namespace cb {

class Client;
class Connection;
class ConnectResponse;

using ClientPtr = std::shared_ptr<Client>;
using ConnectionPtr = std::shared_ptr<Connection>;
using ConnectResponsePtr = std::shared_ptr<ConnectResponse>;

template <typename T> using Callback = std::function<void(const T &)>;

} // namespace cb

#endif // CBERL_TYPES_H
