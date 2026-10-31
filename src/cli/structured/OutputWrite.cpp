#include "cli/structured/OutputWrite.hpp"
#include <stdexcept>

namespace batbox::cli::structured {
event_loop::Deferred<event_loop::Unit> write(
    event_loop::Loop& loop,const Json& message,
    const cli::NdjsonStringifier& stringify,const StdoutSink& sink){
  event_loop::Deferred<event_loop::Unit> result(loop);
  try{
    if(!sink)throw std::invalid_argument("SDK stdout sink required");
    sink(cli::ndjsonSafeStringify(message,stringify)+"\n");
    result.resolve({});
  }catch(...){result.reject(std::current_exception());}
  return result;
}
}
