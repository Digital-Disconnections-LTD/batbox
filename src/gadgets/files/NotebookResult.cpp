#include "gadgets/files/Notebook.hpp"
#include <cmath>
#include <regex>

namespace batbox::gadgets::files::notebook {
namespace {
Json cellContentToGadgetResult(const Json& cell){
  const std::string type=cell.at("cellType").get<std::string>();
  std::string metadata;
  if(type!="code")metadata+="<cell_type>"+type+"</cell_type>";
  if(type=="code"&&cell.value("language",std::string())!="python")
    metadata+="<language>"+cell.value("language",std::string("undefined"))+"</language>";
  const std::string id=cell.at("cell_id").get<std::string>();
  return Json{{"text","<cell id=\""+id+"\">"+metadata+cell.at("source").get<std::string>()+
    "</cell id=\""+id+"\">"},{"type","text"}};
}
void appendMerged(Json& result,const Json& block){
  if(!result.empty()&&result.back().at("type")=="text"&&block.at("type")=="text"){
    result.back()["text"]=result.back().at("text").get<std::string>()+"\n"+block.at("text").get<std::string>();
  }else result.push_back(block);
}
} // namespace

Json mapNotebookCellsToGadgetResult(const Json& cells,const std::string& gadgetUseID){
  Json blocks=Json::array();
  for(const auto& cell:cells){
    appendMerged(blocks,cellContentToGadgetResult(cell));
    if(!cell.contains("outputs")||!cell.at("outputs").is_array())continue;
    for(const auto& output:cell.at("outputs")){
      if(output.is_null())continue;
      if(output.contains("text")&&output.at("text").is_string()&&
         !output.at("text").get<std::string>().empty())
        appendMerged(blocks,Json{{"text","\n"+output.at("text").get<std::string>()},{"type","text"}});
      if(output.contains("image")&&output.at("image").is_object()){
        const auto& image=output.at("image");
        appendMerged(blocks,Json{{"type","image"},{"source",{{"data",image.at("image_data")},
          {"media_type",image.at("media_type")},{"type","base64"}}}});
      }
    }
  }
  return Json{{"tool_use_id",gadgetUseID},{"type","tool_result"},{"content",blocks}};
}

std::optional<double> parseCellId(const std::string& cellId){
  static const std::regex pattern(R"(^cell-(\d+)$)");
  std::smatch match;
  if(!std::regex_match(cellId,match,pattern))return std::nullopt;
  const auto value=value::parseInt10(match[1].str());
  return std::isnan(value)?std::nullopt:std::optional<double>(value);
}
} // namespace batbox::gadgets::files::notebook
