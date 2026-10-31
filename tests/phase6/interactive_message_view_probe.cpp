#include "message_view_tree.hpp"
#include "interactive/message_view/Metadata.hpp"
#include <iostream>
using namespace message_view_fixture;
int main(){
  Json cases;std::cin>>cases;Json out=Json::array();
  for(const auto& spec:cases){
    Json errors=Json::array();MessageImports imports;
    imports.feature=[&](const std::string& name){
      const auto features=spec.value("features",std::vector<std::string>{});
      return std::find(features.begin(),features.end(),name)!=features.end();};
    imports.fullscreen=[&]{return spec.value("fullscreen",false);};
    imports.isSnipBoundary=[](const Json& message){return field(message,"subtype")=="snip_boundary";};
    imports.isSnipMarker=[](const Json& message){return field(message,"subtype")=="snip_marker";};
    imports.isConnectorTextBlock=[](const Json& block){return field(block,"type")=="connector_text";};
    imports.isAdvisorBlock=[](const Json& block){return block.value("advisor",false);};
    imports.logError=[&](const std::string& error){errors.push_back(error);};
    const auto action=spec.value("action",std::string("message"));Json result;
    if(action=="message")result=messageTree(prepareMessage(renderItem(spec.at("message")),options(spec),imports));
    else if(action=="row")result=rowTree(prepareRow(row(spec),imports));
    else if(action=="message_equal"){
      auto before=MessagePlan(renderItem(spec.at("message")),options(spec));
      auto nextSpec=spec;nextSpec["options"].update(spec.value("next",Json::object()));
      if(spec.contains("nextMessage"))nextSpec["message"]=spec.at("nextMessage");
      auto after=MessagePlan(renderItem(nextSpec.at("message")),options(nextSpec));
      result=sameMessageView(before,after);
    }else if(action=="row_equal"){
      auto before=row(spec);auto nextSpec=spec;
      nextSpec["options"].update(spec.value("next",Json::object()));auto after=row(nextSpec);
      if(!spec.value("newMessage",false))after.item=before.item;
      result=sameRowView(before,after);
    }else if(action=="thinking")result=hasThinkingContent(spec.at("message"));
    else if(action=="metadata"){
      MetadataImports meta{[](const std::string&){return std::string("04:05 PM");},
        [](const std::string& text){return static_cast<int>(text.size());}};
      const auto serialize=[](const std::optional<MetadataPart>& part)->Json{
        return part?node("Box",{{"minWidth",part->minWidth}},Json::array({
          node("Text",{{"dimColor",true}},Json::array({part->text}))})):Json(nullptr);};
      const bool transcript=options(spec).transcriptMode;
      result={{"time",serialize(timestampMetadata(spec.at("message"),transcript,meta))},
              {"model",serialize(modelMetadata(spec.at("message"),transcript,meta))}};
    }
    out.push_back({{"result",result},{"errors",errors}});
  }
  std::cout<<out.dump()<<'\n';
}
