#include "transport/RetryAdapter.hpp"
#include "transport/Errors.hpp"
namespace batbox::transport {
namespace {
retry::ErrorRecord fromAPI(const APIError& error,retry::ErrorRecord::Kind kind) {
  retry::ErrorRecord record;
  record.kind=kind;record.status=error.status;record.body=error.body;
  record.headers=error.headers;record.message=error.what();record.name=error.name();
  if (const auto* connection=dynamic_cast<const APIConnectionError*>(&error)) record.code=connection->code;
  return record;
}
} // namespace
retry::ErrorRecord errorRecordFromException(std::exception_ptr failure) {
  try { if(failure)std::rethrow_exception(failure); }
  catch(const retry::RetryFailure& e){return e.error;}
  catch(const APIUserAbortError& e){return fromAPI(e,retry::ErrorRecord::Kind::Abort);}
  catch(const APIConnectionError& e){return fromAPI(e,retry::ErrorRecord::Kind::Connection);}
  catch(const APIError& e){return fromAPI(e,retry::ErrorRecord::Kind::Api);}
  catch(const std::exception& e){retry::ErrorRecord record;record.message=e.what();return record;}
  catch(...){retry::ErrorRecord record;record.message="Unknown native exception";return record;}
  return {};
}
api_errors::Failure apiFailureFromException(std::exception_ptr failure) {
  api_errors::Failure result;
  result.error=errorRecordFromException(failure);
  try { if(failure)std::rethrow_exception(failure); }
  catch(const APIConnectionTimeoutError&){result.isConnectionTimeout=true;}
  catch(...){}
  return result;
}
} // namespace batbox::transport
