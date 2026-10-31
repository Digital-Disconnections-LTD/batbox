#include "gadgets/files/Notebook.hpp"
#include <stdexcept>

namespace batbox::gadgets::files::notebook {
namespace {
constexpr std::size_t largeOutputThreshold=10000;
std::string joined(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_array()){
    std::string out;for(const auto& part:value)out+=part.get<std::string>();return out;
  }
  return "";
}
std::string removeWhitespace(const std::string& value){
  std::string out;out.reserve(value.size());
  for(unsigned char c:value)if(c!=' '&&c!='\t'&&c!='\r'&&c!='\n'&&c!='\f'&&c!='\v')out+=static_cast<char>(c);
  return out;
}
std::string processOutputText(const Json& text,const Services& services){
  if(text.is_null()||(text.is_string()&&text.get<std::string>().empty()))return "";
  if(!services.formatOutput)throw std::invalid_argument("Missing notebook binding: formatOutput");
  return services.formatOutput(joined(text));
}
Json extractImage(const Json& data){
  for(const auto& mediaType:{"image/png","image/jpeg"})
    if(data.contains(mediaType)&&data.at(mediaType).is_string())
      return Json{{"image_data",removeWhitespace(data.at(mediaType).get<std::string>())},{"media_type",mediaType}};
  return Json();
}
Json processOutput(const Json& output,const Services& services){
  const auto type=output.at("output_type").get<std::string>();
  if(type=="stream")return Json{{"output_type",type},
    {"text",processOutputText(output.value("text",Json()),services)}};
  if(type=="execute_result"||type=="display_data"){
    const auto data=output.value("data",Json());
    Json value={{"output_type",type},{"text",processOutputText(
      data.is_object()?data.value("text/plain",Json()):Json(),services)}};
    if(data.is_object()){
      const auto image=extractImage(data);
      if(!image.is_null())value["image"]=image;
    }
    return value;
  }
  if(type=="error"){
    std::string traceback;
    if(output.contains("traceback")&&output.at("traceback").is_array()){
      for(const auto& line:output.at("traceback")){
        if(!traceback.empty())traceback+='\n';
        traceback+=line.get<std::string>();
      }
    }
    const std::string raw=output.value("ename",std::string("undefined"))+": "+
      output.value("evalue",std::string("undefined"))+"\n"+traceback;
    return Json{{"output_type",type},{"text",processOutputText(raw,services)}};
  }
  return Json();
}
bool isLargeOutputs(const Json& outputs){
  std::size_t size=0;
  for(const auto& value:outputs){
    if(value.is_null())continue;
    if(value.contains("text")&&value.at("text").is_string())
      size+=value::utf16CodeUnits(value.at("text").get<std::string>()).size();
    if(value.contains("image")&&value.at("image").is_object())
      size+=value::utf16CodeUnits(value.at("image").at("image_data").get<std::string>()).size();
    if(size>largeOutputThreshold)return true;
  }
  return false;
}
Json processCell(const Json& cell,std::size_t index,const std::string& codeLanguage,
                 bool includeLargeOutputs,const Services& services){
  const std::string type=cell.at("cell_type").get<std::string>();
  const std::string id=cell.contains("id")&&!cell.at("id").is_null()
    ? cell.at("id").get<std::string>() : "cell-"+std::to_string(index);
  Json data={{"cellType",type},{"source",joined(cell.at("source"))}};
  if(type=="code"){
    if(cell.contains("execution_count")&&!cell.at("execution_count").is_null()&&
       cell.at("execution_count")!=0)data["execution_count"]=cell.at("execution_count");
  }
  data["cell_id"]=id;
  if(type=="code"){
    data["language"]=codeLanguage;
    if(cell.contains("outputs")&&cell.at("outputs").is_array()&&!cell.at("outputs").empty()){
      Json outputs=Json::array();
      for(const auto& item:cell.at("outputs"))outputs.push_back(processOutput(item,services));
      if(!includeLargeOutputs&&isLargeOutputs(outputs))
        outputs=Json::array({Json{{"output_type","stream"},{"text",
          "Outputs are too large to include. Use "+services.bashGadgetName+
          " with: cat <notebook_path> | jq '.cells["+std::to_string(index)+"].outputs'"}}});
      data["outputs"]=outputs;
    }
  }
  return data;
}
} // namespace

Json readNotebook(const std::string& notebookPath,const Services& services,
                  const std::optional<std::string>& cellId){
  const auto full=filesystem::expandPath(notebookPath,services.paths);
  const auto content=filesystem::readFileBytes(full);
  const auto notebook=value::parse(content);
  const auto& metadata=notebook.at("metadata");
  const auto languageInfo=metadata.value("language_info",Json());
  const std::string language=languageInfo.is_object()&&languageInfo.contains("name")&&
    !languageInfo.at("name").is_null() ? languageInfo.at("name").get<std::string>() : "python";
  const auto& cells=notebook.at("cells");
  Json result=Json::array();
  if(cellId){
    for(std::size_t i=0;i<cells.size();++i){
      if(cells.at(i).value("id",Json())==*cellId){
        result.push_back(processCell(cells.at(i),i,language,true,services));return result;
      }
    }
    throw std::runtime_error("Cell with ID \""+*cellId+"\" not found in notebook");
  }
  for(std::size_t i=0;i<cells.size();++i)result.push_back(processCell(cells.at(i),i,language,false,services));
  return result;
}
} // namespace batbox::gadgets::files::notebook
