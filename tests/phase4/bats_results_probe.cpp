#include "bats/BatGadgetResults.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static Json run(const Json& item){
 Json trace=Json::array();const Json& messages=field(item,"messages");
 ba::BatResultImports imports;
 imports.getLastAssistantMessage=[&](const Json& all){
  trace.push_back(Json::array({"lastAssistant"}));
  if(all.is_array())for(auto it=all.rbegin();it!=all.rend();++it)
   if(field(*it,"type")=="assistant")return *it;
  return Json(nullptr);};
 imports.getTokenCountFromUsage=[&](const Json& usage){
  trace.push_back(Json::array({"tokens",usage}));
  const Json& input=field(usage,"input_tokens");
  const Json& output=field(usage,"output_tokens");
  return (input.is_number()?input.get<double>():0.0)+
    (output.is_number()?output.get<double>():0.0);};
 imports.extractTextContent=[&](const Json& blocks,const std::string& sep){
  trace.push_back(Json::array({"extract",blocks,sep}));
  std::string out;
  if(blocks.is_array())for(const Json& block:blocks)if(field(block,"type")=="text"){
   if(!out.empty())out+=sep;
   const Json& text=field(block,"text");
   if(text.is_string())out+=text.get<std::string>();}
  return out;};
 imports.nowMillis=[&]{trace.push_back(Json::array({"now"}));return std::int64_t(1000);};
 Json names=Json::array();if(messages.is_array())for(const Json& message:messages){
  auto name=ba::getLastGadgetUseName(message);
  names.push_back(name?Json(*name):Json(nullptr));}
 const auto partial=ba::extractPartialResult(messages,imports);
 Json final=nullptr,error=nullptr;
 try{ba::BatResultMetadata metadata;
  metadata.poetry="prompt";metadata.resolvedBatModel="model";
  metadata.batType="worker";metadata.startTime=900;
  final=ba::finalizeBatGadget(messages,"agent-id",metadata,imports);
 }catch(const std::exception& exception){error=exception.what();}
 return Json{{"trace",trace},{"count",ba::countGadgetUses(messages)},
  {"lastName",names},{"partial",partial?Json(*partial):Json(nullptr)},
  {"final",final},{"error",error}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{
 std::cout<<value::stringify(run(value::parse(line)))<<'\n';
 }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
