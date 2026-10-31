#pragma once
#include "query_engine/Types.hpp"
namespace batbox::query_engine {
bool isResultSuccessful(const MessageRef& message,const Json& stopReason=nullptr);
}
