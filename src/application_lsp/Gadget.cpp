#include "application_lsp/Runtime.hpp"
#include "application_lsp/Symbol.hpp"
#include "runtime/AsyncLaunch.hpp"
#include <filesystem>
#include <set>
#include <stdexcept>

namespace batbox::application_lsp {
namespace {
const std::string description=R"(Interact with Language Server Protocol (LSP) servers to get code intelligence features.

Supported operations:
- goToDefinition: Find where a symbol is defined
- findReferences: Find all references to a symbol
- hover: Get hover information (documentation, type info) for a symbol
- documentSymbol: Get all symbols (functions, classes, variables) in a document
- workspaceSymbol: Search for symbols across the entire workspace
- goToImplementation: Find implementations of an interface or abstract method
- prepareCallHierarchy: Get call hierarchy item at a position (functions/methods)
- incomingCalls: Find all functions/methods that call the function at a position
- outgoingCalls: Find all functions/methods called by the function at a position

All operations require:
- filePath: The file to operate on
- line: The line number (1-based, as shown in editors)
- character: The character offset (1-based, as shown in editors)

LSP servers must be configured for the file type. If no server is available, an error will be returned.)";
std::future<std::string> readyString(std::string value){
  return runtime::contextAsync(std::launch::deferred,[value=std::move(value)]{return value;});
}
std::future<gadgets::ValidationResult> readyValidation(gadgets::ValidationResult value){
  return runtime::contextAsync(std::launch::deferred,[value=std::move(value)]{return value;});
}
gadgets::ValidationResult validate(const Json& input,const std::filesystem::path& cwd,bool checkFile=true){
  gadgets::ValidationResult result;
  const static std::set<std::string> operations={"goToDefinition","findReferences","hover",
    "documentSymbol","workspaceSymbol","goToImplementation","prepareCallHierarchy",
    "incomingCalls","outgoingCalls"};
  auto invalid=[&](std::string message,int code){result.result=false;result.message=std::move(message);
    result.errorCode=code;return result;};
  if(!input.is_object()||input.size()!=4||!input.contains("operation")||
     !input.at("operation").is_string()||!operations.contains(input.at("operation").get<std::string>())||
     !input.contains("filePath")||!input.at("filePath").is_string()||
     !input.contains("line")||!input.at("line").is_number_integer()||input.at("line").get<int>()<=0||
     !input.contains("character")||!input.at("character").is_number_integer()||input.at("character").get<int>()<=0)
    return invalid("Invalid input: expected operation, filePath, positive line and positive character",3);
  if(!checkFile)return result;
  auto requested=input.at("filePath").get<std::string>();
  auto path=std::filesystem::path(requested);
  if(!path.is_absolute())path=cwd/path;
  // Skip UNC path probing during validation.
  if(requested.starts_with("//")||requested.starts_with("\\\\"))return result;
  std::error_code error;
  auto status=std::filesystem::status(path,error);
  if(error==std::errc::no_such_file_or_directory)
    return invalid("File does not exist: "+requested,1);
  if(error)return invalid("Cannot access file: "+requested+". "+error.message(),4);
  if(!std::filesystem::is_regular_file(status))return invalid("Path is not a file: "+requested,2);
  return result;
}
std::string stringField(const Json& input,const char* key){
  return input.is_object()&&input.contains(key)&&input.at(key).is_string()?input.at(key).get<std::string>():"";
}
}
gadgets::Gadget Runtime::makeGadget(std::function<std::future<Json>(const Json&,const Json&)> permission){
  auto self=shared_from_this();
  gadgets::Gadget gadget;
  gadget.name="LSP";gadget.searchHint="code intelligence (definitions, references, symbols, hover)";
  gadget.maxResultSizeChars=100000;gadget.isLsp=true;gadget.shouldDefer=true;
  gadget.inputJSONSchema={{"type","object"},{"additionalProperties",false},
    {"required",Json::array({"operation","filePath","line","character"})},
    {"properties",{{"operation",{{"type","string"},{"enum",Json::array({
      "goToDefinition","findReferences","hover","documentSymbol","workspaceSymbol",
      "goToImplementation","prepareCallHierarchy","incomingCalls","outgoingCalls"})}}},
      {"filePath",{{"type","string"}}},{"line",{{"type","integer"},{"minimum",1}}},
      {"character",{{"type","integer"},{"minimum",1}}}}}};
  gadget.inputSchema=gadget.inputJSONSchema;
  gadget.outputSchema={{"type","object"},{"properties",{{"operation",{{"type","string"}}},
    {"result",{{"type","string"}}},{"filePath",{{"type","string"}}},
    {"resultCount",{{"type","integer"}}},{"fileCount",{{"type","integer"}}}}}};
  gadget.parseInput=[cwd=roost()](const Json& input){
    auto outcome=validate(input,cwd,false);if(!outcome.result)throw std::invalid_argument(*outcome.message);
    return input;
  };
  gadget.description=[](const Json&,const Json&){return readyString(description);};
  gadget.poetry=[](const gadgets::PoetryContext&){return readyString(description);};
  gadget.isEnabled=[self]{return self->available();};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.isDestructive=[](const Json&){return false;};
  gadget.getPath=[cwd=roost()](const Json& input){
    auto path=std::filesystem::path(stringField(input,"filePath"));
    return (path.is_absolute()?path:cwd/path).lexically_normal().string();
  };
  gadget.userFacingName=[](const Json&){return std::string("LSP");};
  gadget.getGadgetUseSummary=[](const Json& input)->Json{return stringField(input,"operation")+" "+stringField(input,"filePath");};
  gadget.validateInput=[cwd=roost()](const Json& input,const Json&){return readyValidation(validate(input,cwd));};
  if(permission)gadget.checkPermissions=std::move(permission);
  gadget.call=[self](const Json& input,const gadgets::GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const gadgets::GadgetProgress&)>&){return self->callGadget(input,context);};
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    return Json{{"tool_use_id",id},{"type","tool_result"},{"content",data.value("result",std::string{})}};
  };
  gadget.renderGadgetUseMessage=[cwd=roost()](const Json& input,const Json&)->Json{
    auto path=std::filesystem::path(stringField(input,"filePath"));
    if(!path.is_absolute())path=cwd/path;
    auto line=input.value("line",0),character=input.value("character",0);
    auto symbol=symbolAtPosition(path,line-1,character-1);
    return "LSP "+stringField(input,"operation")+": "+stringField(input,"filePath")+
      (symbol?" ("+*symbol+")":"")+":"+std::to_string(line)+":"+std::to_string(character);
  };
  // Result renderers receive (output, progress, options).
  gadget.renderGadgetResultMessage=[](const Json& data,const Json&,const Json&)->Json{
    if(data.is_object()&&data.contains("resultCount")&&data.at("resultCount").is_number_integer()){
      auto count=data.at("resultCount").get<int>();
      auto operation=stringField(data,"operation");
      if(operation=="hover"&&count>0)return "Hover info available";
      return "Found "+std::to_string(count)+" "+(count==1?"result":"results");
    }
    return data.value("result",std::string{});
  };
  gadget.renderGadgetUseErrorMessage=[](const Json& error,const Json&)->Json{return error.dump();};
  return gadgets::buildGadget(std::move(gadget));
}
}
