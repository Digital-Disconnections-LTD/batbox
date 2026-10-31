#pragma once
#include "cli/remote/CcrRuntime.hpp"
#include "transport/Client.hpp"

namespace batbox::cli::remote {
std::function<Deferred<CcrHttpResponse>(const std::string&,
  const std::string&,const Json&,const Json&,double)>
makeIndigenousCcrHttp(event_loop::Loop& loop,transport::ClientConfig config);
}
