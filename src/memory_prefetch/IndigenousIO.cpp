#include "memory_prefetch/IndigenousIO.hpp"
#include <optional>

namespace batbox::memory_prefetch {
event_loop::Deferred<std::vector<instructions::MemoryHeader>>
scanMemoryFilesAsync(const std::string& dir,std::stop_token signal,
  event_loop::Loop& loop){
  event_loop::Deferred<std::vector<instructions::MemoryHeader>> out(loop);
  auto value=std::make_shared<std::vector<instructions::MemoryHeader>>();
  loop.queueWork([dir,signal,value]{
    *value=instructions::scanMemoryFiles(dir,signal);
  },[out,value](std::exception_ptr error){
    if(error)out.reject(error);else out.resolve(std::move(*value));
  });
  return out;
}
event_loop::Deferred<filesystem::ReadFileRangeResult>
readMemoryRangeAsync(const std::string& path,std::stop_token signal,
  event_loop::Loop& loop){
  event_loop::Deferred<filesystem::ReadFileRangeResult> out(loop);
  auto value=std::make_shared<filesystem::ReadFileRangeResult>();
  loop.queueWork([path,signal,value]{
    *value=filesystem::readFileInRange(path,0,200,4096,signal,true);
  },[out,value](std::exception_ptr error){
    if(error)out.reject(error);else out.resolve(std::move(*value));
  });
  return out;
}
} // namespace batbox::memory_prefetch
