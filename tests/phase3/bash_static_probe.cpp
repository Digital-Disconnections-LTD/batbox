#include "shell/StaticCommand.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  using namespace batbox;
  const auto id=[](){return std::string("task1");};
  const std::vector<shell::StaticCommand> commands={
    shell::createAbortedCommand(id),shell::createAbortedCommand(id,"bg1"),
    shell::createAbortedCommand(id,{},"custom",126),
    shell::createFailedCommand(id,"directory missing")};
  Json output=Json::array();
  for(const auto& command:commands){output.push_back({{"taskId",command.missionId},
    {"status",command.status==shell::CommandStatus::Killed?"killed":"completed"},
    {"result",command.result.toJson()},{"background",command.background("x")}});}
  std::cout<<value::stringify(output)<<'\n';
}
