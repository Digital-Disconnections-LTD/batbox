#include <cstdlib>
#include "shell/LoopMissionOutput.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unistd.h>
#include <vector>
using namespace batbox;
using namespace batbox::event_loop;
using namespace batbox::shell;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
  std::string pattern="/tmp/batbox-loop-poller-XXXXXX";
  std::vector<char> chars(pattern.begin(),pattern.end());chars.push_back('\0');
  const auto made=::mkdtemp(chars.data());require(made,"mkdtemp failed");
  const std::filesystem::path root=made;
  try{
    const auto file=(root/"output").string();
    {std::ofstream output(file);output<<"first\nsecond\n";}
    Loop loop;MissionOutputPoller poller;std::vector<PollSample> samples;int failures=0;
    LoopMissionOutput binding(loop,poller,"task",file,
      [&](const PollSample& sample){
        require(loop.isOwnerThread(),"poller callback left owner strand");
        samples.push_back(sample);
      },[&](std::exception_ptr){++failures;});
    binding.start();poller.tick();loop.runReady();
    require(samples.size()==1&&samples[0].allLines.find("second")!=std::string::npos,
      "file poll sample did not reach owner");
    // A worker sample queued before stop must not run after polling cleanup.
    poller.tick();binding.stop();loop.runReady();
    require(samples.size()==1&&failures==0,"stopped poller delivered a stale sample");
    const auto begin=std::chrono::steady_clock::now();loop.run();
    require(std::chrono::steady_clock::now()-begin<std::chrono::milliseconds(100),
      "unreferenced poller kept owner loop alive");
  }catch(...){std::filesystem::remove_all(root);throw;}
  std::filesystem::remove_all(root);
}
