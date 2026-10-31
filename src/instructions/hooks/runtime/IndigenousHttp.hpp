#pragma once
#include "instructions/hooks/runtime/HttpHook.hpp"

namespace batbox::instructions::hooks::runtime {
// Indigenous axios.post transport import. HTTP policy/proxy selection runs in
// execHttpHook; this adapter performs one POST with redirects disabled.
std::function<event_loop::Deferred<HttpTransportResponse>(HttpTransportRequest)>
indigenousHttpPost(event_loop::Loop& loop);
} // namespace batbox::instructions::hooks::runtime
