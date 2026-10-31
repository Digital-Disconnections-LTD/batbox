#pragma once
#include <stdexcept>
#include <stop_token>
namespace batbox {
struct ConnectorAbortError:std::runtime_error {
  ConnectorAbortError():std::runtime_error("Operation aborted"){}
};
inline void checkConnectorAbort(std::stop_token signal) {
  if(signal.stop_requested())throw ConnectorAbortError();
}
}
