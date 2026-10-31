#include "cleanup/Retention.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace batbox;
int main(){
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("batbox-retention-"+std::to_string(::getpid()));
  const auto errors=root/"errors",logs=root/"logs",mcp=logs/"mcp-logs-a";
  fs::create_directories(errors);fs::create_directories(mcp);
  const auto oldError=errors/"2024-01-02T03-04-05-006Z.log";
  const auto freshError=errors/"2026-10-27T03-04-05-006Z.log";
  const auto oldMessage=mcp/"2024-01-02T03-04-05-006Z.txt";
  for(const auto& file:{oldError,freshError,oldMessage}){std::ofstream out(file);out<<"content";}
  assert(cleanup::filenameToIsoString(oldMessage.filename().string())=="2024-01-02T03:04:05.006Z");
  assert(!cleanup::indigenousLogDateMs("invalid"));
  event_loop::Loop loop;int diagnostics=0;
  auto services=cleanup::indigenousRetentionServices(loop,[&](std::exception_ptr){++diagnostics;});
  auto pending=cleanup::cleanupOldMessageFiles(loop,errors.string(),logs.string(),
    *cleanup::indigenousLogDateMs("2026-10-01T00:00:00.000Z"),services);
  std::optional<cleanup::Result> result;
  pending.then([&](const cleanup::Result* value,const std::exception_ptr& error){
    assert(!error);result=*value;
  });
  loop.run();
  assert(result&&result->errors==1&&result->messages==1);
  assert(diagnostics==0);
  assert(!fs::exists(oldError)&&!fs::exists(oldMessage)&&!fs::exists(mcp));
  assert(fs::exists(freshError));
  fs::remove_all(root);
}
