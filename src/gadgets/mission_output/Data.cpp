#include "gadgets/mission_output/MissionOutput.hpp"

namespace batbox::mission_output {
Json getMissionOutputData(const shell_missions::MissionRecord& mission,const shell_missions::Roster& roster){
  auto output=roster.readOutput(mission);
  Json result={{"task_id",mission.id},{"task_type",mission.type},{"status",mission.status},
    {"description",mission.description},{"output",output}};
  if(mission.type=="local_bash"){
    result["exitCode"]=mission.exitCode?Json(*mission.exitCode):Json(nullptr);
    return result;
  }
  if(mission.type=="local_agent"){
    if(mission.result&&!mission.result->empty()){result["result"]=*mission.result;result["output"]=*mission.result;}
    else result["result"]=output;
    if(mission.poetry)result["prompt"]=*mission.poetry;
    if(mission.error)result["error"]=*mission.error;
    return result;
  }
  if(mission.type=="remote_agent"){
    result["prompt"]=mission.command;
    return result;
  }
  return result;
}
} // namespace batbox::mission_output
