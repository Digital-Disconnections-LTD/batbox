#include "interactive/cards_remaining/MessageLeaves.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_remaining {
namespace {
void require(const auto& callback,const char* name){
  if(!callback)throw std::logic_error(std::string(name)+" import required");
}
std::string stringField(const Json& value,const char* key){
  if(!value.is_object())return {};
  const auto found=value.find(key);
  return found!=value.end()&&found->is_string()?found->get<std::string>():"";
}
std::string jsText(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_number())return value::numberToString(value.get<double>());
  return value::stringify(value);
}
}

ImageCard classifyUserImage(std::optional<int> imageID,bool addMargin,
                            const ImageServices& services){
  ImageCard card;
  card.addMargin=addMargin;
  card.label=imageID&&*imageID!=0?"[Image #"+std::to_string(*imageID)+"]":
    "[Image]";
  if(imageID&&*imageID!=0){
    require(services.getStoredImagePath,"stored image path");
    const auto path=services.getStoredImagePath(*imageID);
    if(path&&!path->empty()){
      require(services.supportsHyperlinks,"terminal hyperlink support");
      if(services.supportsHyperlinks()){
        require(services.pathToFileURL,"file URL conversion");
        card.fileURL=services.pathToFileURL(*path);
      }
    }
  }
  return card;
}

CompactCard classifyCompactSummary(const Json& message,
    const std::string& screen,const CompactServices& services){
  require(services.getUserMessageText,"user message text");
  const auto text=services.getUserMessageText(message);
  CompactCard card;
  card.transcriptMode=screen=="transcript";
  const auto metadata=message.find("summarizeMetadata");
  card.metadata=metadata!=message.end()&&value::truthy(*metadata);
  if(card.metadata){
    card.title="Summarized conversation";
    if(card.transcriptMode)card.body=text;
    else{
      const auto count=metadata->find("messagesSummarized");
      const auto direction=stringField(*metadata,"direction");
      card.body="Summarized "+(count==metadata->end()?"undefined":jsText(*count))+
        " messages "+(direction=="up_to"?"up to this point":"from this point");
      const auto context=metadata->find("userContext");
      if(context!=metadata->end()&&value::truthy(*context))
        card.context="Context: “"+jsText(*context)+"”";
      require(services.shortcutHint,"expand-history shortcut");
      card.shortcut=services.shortcutHint("expand history");
    }
  }else{
    card.title="Compact summary";
    if(card.transcriptMode)card.body=text;
    else{
      require(services.shortcutHint,"expand shortcut");
      card.shortcut=services.shortcutHint("expand");
    }
  }
  return card;
}

std::string compactBoundaryText(const std::function<std::string()>& historyShortcut){
  require(historyShortcut,"history shortcut");
  return "✻ Conversation compacted ("+historyShortcut()+" for history)";
}

AdvisorCard classifyAdvisor(const Json& block,bool addMargin,
    const GadgetIDs& resolvedIDs,const GadgetIDs& erroredIDs,
    bool shouldAnimate,bool verbose,const std::optional<std::string>& advisorModel,
    const AdvisorServices& services){
  AdvisorCard card;
  card.addMargin=addMargin;
  card.verbose=verbose;
  if(stringField(block,"type")=="server_tool_use"){
    card.kind=AdvisorCard::Kind::GadgetUse;
    const auto input=block.find("input");
    if(input!=block.end()&&value::truthy(*input)&&input->is_object()&&!input->empty())
      card.input=value::stringify(*input);
    const auto id=stringField(block,"id");
    card.resolved=resolvedIDs.contains(id);
    card.errored=erroredIDs.contains(id);
    card.animate=shouldAnimate;
    if(advisorModel&& !advisorModel->empty()){
      require(services.renderModelName,"advisor model display name");
      card.modelName=services.renderModelName(*advisorModel);
    }
    return card;
  }
  const auto content=block.find("content");
  if(content==block.end()||!content->is_object())return card;
  const auto type=stringField(*content,"type");
  if(type=="advisor_tool_result_error"){
    card.kind=AdvisorCard::Kind::Error;
    const auto error=content->find("error_code");
    card.text="Advisor unavailable ("+
      (error==content->end()?"undefined":jsText(*error))+")";
  }else if(type=="advisor_result"){
    card.kind=AdvisorCard::Kind::Result;
    if(verbose)card.text=stringField(*content,"text");
    else{
      card.text="Advisor has reviewed the conversation and will apply the feedback";
      require(services.expandHint,"advisor expand hint");
      card.text+=" "+services.expandHint();
    }
  }else if(type=="advisor_redacted_result"){
    card.kind=AdvisorCard::Kind::Redacted;
    card.text="Advisor has reviewed the conversation and will apply the feedback";
  }
  return card;
}

PartnerHeaderCard classifyPartnerHeader(
    const PartnerHeaderServices& services){
  require(services.readViewedPartner,"viewed Partner Mission");
  const auto viewed=services.readViewedPartner();
  if(!viewed)return {};
  require(services.exitShortcut,"Partner return shortcut");
  const auto identity=viewed->value("identity",Json::object());
  return {true,stringField(identity,"agentName"),stringField(identity,"color"),
    stringField(*viewed,"prompt"),services.exitShortcut()};
}
}
