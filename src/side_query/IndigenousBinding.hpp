#pragma once
#include "side_query/SideQuery.hpp"
#include "client_factory/ClientFactory.hpp"
#include "transport/SdkRetry.hpp"

namespace batbox::side_query {
void bindIndigenousClientFactory(event_loop::Loop& loop,Services& target,
  client_factory::Services factory,transport::SdkRetryClock clock=
    transport::indigenousSdkRetryClock());
} // namespace batbox::side_query
