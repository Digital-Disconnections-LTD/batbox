#include "cron/CronScheduler.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::cron {
std::string buildMissedMissionNotification(
  const std::vector<CronMissionRef>& missed,
  const std::function<std::string(const std::string&)>& cronToHuman,
  const std::function<std::string(double)>& localDateTime){
  if(!cronToHuman||!localDateTime)
    throw std::invalid_argument("Cron notification formatting bindings required");
  const bool plural=missed.size()>1;
  const std::string header=
    std::string("The following one-shot scheduled task")+
    (plural?"s were":" was")+" missed while Batbox was not running. "+
    (plural?"They have":"It has")+
    " already been removed from .batbox/scheduled_tasks.json.\n\n"+
    "Do NOT execute "+(plural?"these prompts":"this prompt")+" yet. "+
    "First use the AskUserQuestion tool to ask whether to run "+
    (plural?"each one":"it")+" now. "+
    "Only execute if the user confirms.";
  std::vector<std::string> blocks;
  for(const auto& mission:missed){
    const auto meta="["+cronToHuman(mission->cron)+", created "+
      localDateTime(mission->createdAt)+"]";
    std::size_t longest=0,run=0;
    for(const char ch:mission->poetry){
      if(ch=='`'){++run;longest=std::max(longest,run);}
      else run=0;
    }
    const std::string fence(std::max<std::size_t>(3,longest+1),'`');
    blocks.push_back(meta+"\n"+fence+"\n"+mission->poetry+"\n"+fence);
  }
  std::string result=header+"\n\n";
  for(std::size_t index=0;index<blocks.size();++index){
    if(index)result+="\n\n";
    result+=blocks[index];
  }
  return result;
}
}
