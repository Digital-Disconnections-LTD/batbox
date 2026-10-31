#include "gadgets/files/FileReadInternal.hpp"
#include "gadgets/files/ActiveTriggers.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace batbox::gadgets::files {
Json createMetaReadMessage(const Json& content,FileReadServices& services){
  requireReadBinding(services.messageServices!=nullptr,"messageServices");
  return messages::createUserMessage(Json{{"content",content},{"isMeta",true}},*services.messageServices);
}

void validateReadTokens(const std::string& content,const std::string& extension,
                        std::size_t maxTokens,const FileReadServices& services){
  requireReadBinding(static_cast<bool>(services.roughTokenCountEstimationForFileType),
    "roughTokenCountEstimationForFileType");
  const double estimate=services.roughTokenCountEstimationForFileType(content,extension);
  if(!estimate||estimate<=static_cast<double>(maxTokens)/4.0)return;
  requireReadBinding(static_cast<bool>(services.countTokensWithAPI),"countTokensWithAPI");
  const auto actual=services.countTokensWithAPI(content);
  const double count=actual.value_or(estimate);
  if(count>maxTokens){
    const auto number=[](double value){
      if(std::isfinite(value)&&std::floor(value)==value)return std::to_string(static_cast<long long>(value));
      return value::stringify(Json(value));
    };
    throw MaxFileReadTokenExceededError(count,static_cast<double>(maxTokens),
      "File content ("+number(count)+" tokens) exceeds maximum allowed tokens ("+
      std::to_string(maxTokens)+"). Use offset and limit parameters to read specific portions of the file, or search for specific content instead of reading the whole file.");
  }
}

GadgetResult callReadInner(const ReadArguments& args,const GadgetCallContext& context,FileReadServices& services){
  if(args.extension=="ipynb"){
    requireReadBinding(static_cast<bool>(services.readNotebook),"readNotebook");
    const Json cells=services.readNotebook(args.resolvedPath);
    const std::string cellsJson=value::stringify(cells);
    if(cellsJson.size()>args.maxSizeBytes){
      requireReadBinding(static_cast<bool>(services.formatFileSize),"formatFileSize");
      throw std::runtime_error("Notebook content ("+services.formatFileSize(cellsJson.size())+
        ") exceeds maximum allowed size ("+services.formatFileSize(args.maxSizeBytes)+"). Use "+
        services.bashGadgetName+" with jq to read specific portions:\n"+
        "  cat \""+args.inputPath+"\" | jq '.cells[:20]' # First 20 cells\n"+
        "  cat \""+args.inputPath+"\" | jq '.cells[100:120]' # Cells 100-120\n"+
        "  cat \""+args.inputPath+"\" | jq '.cells | length' # Count total cells\n"+
        "  cat \""+args.inputPath+"\" | jq '.cells[] | select(.cell_type==\"code\") | .source' # All code sources");
    }
    validateReadTokens(cellsJson,args.extension,args.maxTokens,services);
    const auto mtime=filesystem::getFileModificationTime(args.resolvedPath);
    services.readFileState->set(args.fullPath,ReadSnapshot{cellsJson,mtime,false,
      static_cast<double>(args.offset),args.limit?std::optional<double>(*args.limit):std::nullopt});
    recordNestedMemoryTrigger(services,args.fullPath);
    return GadgetResult{Json{{"type","notebook"},{"file",{{"filePath",args.inputPath},{"cells",cells}}}}};
  }
  if(args.extension=="png"||args.extension=="jpg"||args.extension=="jpeg"||
     args.extension=="gif"||args.extension=="webp"){
    const Json data=readImageWithTokenBudget(args.resolvedPath,args.maxTokens,std::nullopt,services.image);
    recordNestedMemoryTrigger(services,args.fullPath);
    GadgetResult result{data};
    const auto& file=data.at("file");
    if(file.contains("dimensions")&&!file.at("dimensions").is_null()){
      requireReadBinding(static_cast<bool>(services.image.createImageMetadataText),"createImageMetadataText");
      const auto metadata=services.image.createImageMetadataText(file.at("dimensions"));
      if(!metadata.empty())result.newMessages=std::vector<Json>{createMetaReadMessage(metadata,services)};
    }
    return result;
  }
  if(isPDFExtension(args.extension))return callReadPDF(args,context,services);
  const auto lineOffset=args.offset==0 ? 0 : args.offset-1;
  const auto range=filesystem::readFileInRange(args.resolvedPath,lineOffset,args.limit,
    args.limit?std::nullopt:std::optional<std::size_t>(args.maxSizeBytes),
    context.stopToken,false,services.range);
  validateReadTokens(range.content,args.extension,args.maxTokens,services);
  services.readFileState->set(args.fullPath,ReadSnapshot{range.content,std::floor(range.mtimeMs),false,
    static_cast<double>(args.offset),args.limit?std::optional<double>(*args.limit):std::nullopt});
  recordNestedMemoryTrigger(services,args.fullPath);
  const auto listeners=services.fileReadListeners;
  for(const auto& listener:listeners)listener(args.resolvedPath,range.content);
  GadgetResult result{Json{{"type","text"},{"file",{{"filePath",args.inputPath},
    {"content",range.content},{"numLines",range.lineCount},
    {"startLine",args.offset},{"totalLines",range.totalLines}}}}};
  requireReadBinding(static_cast<bool>(services.isAutoMemFile),"isAutoMemFile");
  if(services.isAutoMemFile(args.fullPath))
    result.presentationMetadata=Json{{"memoryMtimeMs",range.mtimeMs}};
  return result;
}
} // namespace batbox::gadgets::files
