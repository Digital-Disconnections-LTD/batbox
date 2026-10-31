#include "runtime/AsyncLaunch.hpp"
#include "gadgets/files/FileGadgetsInternal.hpp"
#include "filesystem/FileOps.hpp"
#include <stdexcept>

namespace batbox::gadgets::files {
namespace {
void require(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Missing file tool binding: ")+name);
}
} // namespace
std::string stringValue(const Json& input,const char* key){
  return input.is_object()&&input.contains(key)&&input.at(key).is_string()
    ? input.at(key).get<std::string>() : "";
}
Gadget makeFileGadgetBase(const std::string& name,const std::string& searchHint,
                      double maxResultSize,bool strict,FileGadgetBindings bindings){
  require(static_cast<bool>(bindings.checkPermissions),"checkPermissions");
  require(static_cast<bool>(bindings.matchWildcardPattern),"matchWildcardPattern");
  require(static_cast<bool>(bindings.userFacingName),"userFacingName");
  require(static_cast<bool>(bindings.getGadgetUseSummary),"getToolUseSummary");
  require(static_cast<bool>(bindings.renderUse),"renderUse");
  require(static_cast<bool>(bindings.renderResult),"renderResult");
  require(static_cast<bool>(bindings.renderError),"renderError");
  auto shared=std::make_shared<FileGadgetBindings>(std::move(bindings));
  Gadget gadget;
  gadget.name=name;gadget.searchHint=searchHint;gadget.maxResultSizeChars=maxResultSize;
  if(strict)gadget.strict=true;
  gadget.inputJSONSchema=fileGadgetInputSchema(name);
  gadget.outputSchema=fileGadgetOutputSchema(name);
  gadget.parseInput=[name](const Json& input){return parseFileGadgetInput(name,input);};
  gadget.checkPermissions=shared->checkPermissions;
  gadget.userFacingName=shared->userFacingName;
  gadget.getGadgetUseSummary=shared->getGadgetUseSummary;
  gadget.renderGadgetUseMessage=shared->renderUse;
  gadget.renderGadgetResultMessage=shared->renderResult;
  gadget.renderGadgetUseErrorMessage=shared->renderError;
  gadget.renderGadgetUseRejectedMessage=shared->renderRejected;
  gadget.renderGadgetUseTag=shared->renderTag;
  gadget.isResultTruncated=shared->isResultTruncated;
  gadget.preparePermissionMatcher=[shared,name](const Json& input){
    const std::string source=name=="Glob"||name=="Grep"
      ? stringValue(input,"pattern") : stringValue(input,"file_path");
    return ::batbox::runtime::contextAsync(std::launch::deferred,[shared,source](){
      return std::function<bool(const std::string&)>([shared,source](const std::string& pattern){
        return shared->matchWildcardPattern(pattern,source);});
    });
  };
  const auto action=name=="Read"?"Reading ":name=="Write"?"Writing ":
    name=="Edit"?"Editing ":name=="Glob"?"Finding ":"Searching for ";
  const auto fallback=name=="Read"?"Reading file":name=="Write"?"Writing file":
    name=="Edit"?"Editing file":name=="Glob"?"Finding files":"Searching";
  gadget.getActivityDescription=[shared,action,fallback](const Json& input)->Json{
    const auto summary=shared->getGadgetUseSummary(input);
    return value::truthy(summary)&&summary.is_string()
      ? Json(std::string(action)+summary.get<std::string>()) : Json(fallback);
  };
  return gadget;
}
void bindFilePathBackfill(Gadget& gadget,const filesystem::PathServices& paths){
  gadget.backfillObservableInput=[paths](Json& input){
    if(input.is_object()&&input.contains("file_path")&&input.at("file_path").is_string())
      input["file_path"]=filesystem::expandPath(input.at("file_path").get<std::string>(),paths);
  };
}
} // namespace batbox::gadgets::files
