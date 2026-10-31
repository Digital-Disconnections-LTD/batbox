#include "message_view_fixture.hpp"
#include "interactive/message_list_view/Surface.hpp"
#include <iostream>
using namespace message_view_fixture;
namespace surface=batbox::interactive::message_list_view;
namespace gadgets=batbox::gadgets;
int main(){
  Json cases;std::cin>>cases;Json results=Json::array();
  for(const auto& spec:cases){
    surface::SurfaceState state;const auto action=spec.value("action",std::string("surface"));
    if(action=="progress"){
      Json trace=Json::array();const auto progress=[&](std::optional<surface::ProgressState> value){
        trace.push_back(!value?Json(nullptr):Json(*value==surface::ProgressState::Indeterminate?"indeterminate":"completed"));};
      for(const auto& event:spec.at("events"))state.committed(event.at("enabled"),event.at("active"),progress);
      state.unmounted(progress);results.push_back(trace);continue;
    }
    if(action=="visibility"){
      Json trace=Json::array();std::shared_ptr<const StreamingThinking> current;
      for(const auto& event:spec.at("events")){
        if(!event.value("same",false)){
          current.reset();const auto& value=field(event,"thinking");
          if(value.is_object()){
            auto next=std::make_shared<StreamingThinking>();next->isStreaming=value.value("isStreaming",false);
            if(value.contains("streamingEndedAt"))next->streamingEndedAt=value.at("streamingEndedAt").get<double>();current=next;
          }
        }
        trace.push_back(state.thinkingVisible(current,event.at("now")));
      }
      results.push_back(trace);continue;
    }
    gadgets::Gadget gadget;gadget.name="Read";gadget.isResultTruncated=[&](const Json&){return spec.value("truncated",false);};
    gadgets::Gadgets roster;if(spec.value("tool",false))roster.push_back(&gadget);
    auto lookup=lookups(spec);
    auto owner=std::make_shared<Json>(Json{{"message",{{"content",Json::array({{{"name","Read"}}})}}}});
    lookup->gadgetUseByGadgetUseID["t1"]={owner,0};
    if(action=="clickable"){
      results.push_back(surface::isItemClickable(spec.at("message"),*lookup,roster,
        [](const Json& block){return block.value("advisor",false);}));continue;
    }
    auto prepared=std::make_shared<PreparedMessageList>();prepared->lookups=*lookup;
    prepared->renderable=std::make_shared<RenderItems>();
    for(const auto& message:spec.at("messages"))prepared->renderable->push_back(renderItem(message));
    for(const auto& message:spec.value("normalized",spec.at("messages")))prepared->normalizedMessages.push_back(std::make_shared<Json>(message));
    prepared->hasTruncatedMessages=spec.value("hasTruncated",false);prepared->hiddenMessageCount=spec.value("hiddenCount",0);
    const auto opts=spec.value("options",Json::object());surface::SurfaceOptions o;
    o.screen=opts.value("screen",std::string("prompt"));o.conversationId="conversation";o.columns=opts.value("columns",80);
    o.verbose=opts.value("verbose",false);o.isLoading=opts.value("isLoading",false);
    o.hidePastThinking=opts.value("hidePastThinking",false);o.hideLogo=opts.value("hideLogo",false);
    o.briefOnly=opts.value("briefOnly",false);o.showAllInTranscript=opts.value("showAllInTranscript",false);
    o.disableRenderCap=opts.value("disableRenderCap",false);o.virtualScroll=opts.value("virtualScroll",false);
    if(opts.contains("renderRange"))o.renderRange={{opts.at("renderRange")[0],opts.at("renderRange")[1]}};
    if(opts.contains("toolUiContinueAnimation"))o.gadgetUiContinueAnimation=opts.at("toolUiContinueAnimation").get<bool>();
    o.permissionQueueSize=opts.value("permissionQueueSize",0);o.messageSelectorVisible=opts.value("messageSelectorVisible",false);
    o.inProgressGadgetUseIDs=ids(field(opts,"inProgressToolUseIDs"));o.streamingGadgetUseIDs=ids(field(opts,"streamingToolUseIDs"));
    if(field(opts,"streamingText").is_string())o.streamingText=opts.at("streamingText").get<std::string>();
    if(field(opts,"streamingThinking").is_object()){
      const auto& value=opts.at("streamingThinking");auto thinking=std::make_shared<StreamingThinking>();
      thinking->thinking=value.value("thinking",std::string("reason"));thinking->isStreaming=value.value("isStreaming",false);
      if(value.contains("streamingEndedAt"))thinking->streamingEndedAt=value.at("streamingEndedAt").get<double>();o.streamingThinking=thinking;
    }
    if(opts.contains("cursor"))o.cursor=surface::Cursor{opts.at("cursor").at("uuid"),opts.at("cursor").value("expanded",false)};
    if(opts.contains("unseenDivider"))o.unseenDivider=surface::UnseenDivider{opts.at("unseenDivider").at("firstUnseenUuid"),opts.at("unseenDivider").at("count")};
    for(const auto& key:spec.value("expanded",std::vector<std::string>{}))
      for(const auto& item:*prepared->renderable)if(expandKey(*item.message)==key){state.toggleExpanded(*item.message);break;}
    MessageImports imports;imports.feature=[](const std::string&){return false;};imports.fullscreen=[]{return false;};
    imports.logError=[](const std::string&){};
    const auto plan=surface::prepareSurface(prepared,o,state,spec.value("now",100000.0),imports,
      {[](const std::string& name,const Json&){return name=="Read"||name=="Search";}});
    Json rows=Json::array();
    for(const auto& row:plan.rows){const auto& request=row.request;const auto& options=request.options;
      Json props={{"message",materializeRenderItem(request.item)},{"isUserContinuation",options.isUserContinuation},
        {"hasContentAfter",request.hasContentAfter},{"verbose",options.verbose},{"inProgressToolUseIDs",idsJson(options.inProgressGadgetUseIDs)},
        {"streamingToolUseIDs",idsJson(request.streamingGadgetUseIDs)},{"screen",request.screen},{"canAnimate",request.canAnimate},
        {"lastThinkingBlockId",options.lastThinkingBlockId},{"latestBashOutputUUID",options.latestBashOutputUUID},
        {"columns",options.columns},{"isLoading",request.isLoading}};
      rows.push_back({{"key",row.key},{"selected",row.selected},{"unseen",row.unseenCount?Json(*row.unseenCount):Json(nullptr)},{"props",props}});
    }
    results.push_back({{"lastThinking",plan.lastThinkingBlockId},{"latestBash",plan.latestBashOutputUUID},
      {"logo",plan.showLogo},{"showPrevious",plan.showTruncated},{"hidePrevious",plan.showAll},
      {"streamingText",plan.showStreamingText},{"streamingThinking",plan.showStreamingThinking},{"rows",rows}});
  }
  std::cout<<results.dump()<<'\n';
}
