#include "interactive/navigation/ResubmitText.hpp"
#include "interactive/MessageTags.hpp"
#include "messages/TextContent.hpp"
#include "display/Tags.hpp"

namespace batbox::interactive::navigation {
std::optional<ResubmitText> textForResubmit(const TurnMessageRef& message){
  if(!message||value::property(*message,"type")!="user")return {};
  const auto content=messages::getContentText(
    value::property(value::property(*message,"message"),"content"));
  if(!content)return {};
  const auto bash=extractTag(*content,"bash-input");
  if(bash&&!bash->empty())return ResubmitText{*bash,"bash"};
  const auto cmd=extractTag(*content,"command-name");
  if(cmd&&!cmd->empty()){
    const auto args=extractTag(*content,"command-args").value_or("");
    return ResubmitText{*cmd+" "+args,"prompt"};
  }
  return ResubmitText{display::stripIdeContextTags(*content),"prompt"};
}
} // namespace batbox::interactive::navigation
