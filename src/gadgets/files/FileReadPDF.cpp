#include "gadgets/files/FileReadInternal.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace batbox::gadgets::files {
namespace {
std::string base64(const std::string& bytes) {
  static constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;out.reserve(((bytes.size()+2)/3)*4);
  for(std::size_t i=0;i<bytes.size();i+=3){
    const auto a=static_cast<unsigned char>(bytes[i]);
    const auto b=i+1<bytes.size()?static_cast<unsigned char>(bytes[i+1]):0;
    const auto c=i+2<bytes.size()?static_cast<unsigned char>(bytes[i+2]):0;
    out+=alphabet[a>>2];out+=alphabet[((a&3)<<4)|(b>>4)];
    out+=i+1<bytes.size()?alphabet[((b&15)<<2)|(c>>6)]:'=';
    out+=i+2<bytes.size()?alphabet[c&63]:'=';
  }
  return out;
}
} // namespace

GadgetResult callReadPDF(const ReadArguments& args,const GadgetCallContext&,FileReadServices& services){
  if(args.pages){
    requireReadBinding(static_cast<bool>(services.extractPDFPages),"extractPDFPages");
    const auto extraction=services.extractPDFPages(args.resolvedPath,parsePDFPageRange(*args.pages));
    if(!extraction.at("success").get<bool>())throw std::runtime_error(extraction.at("error").at("message").get<std::string>());
    const auto data=extraction.at("data");
    const auto outputDir=data.at("file").at("outputDir").get<std::string>();
    std::vector<std::filesystem::path> files;
    for(const auto& entry:std::filesystem::directory_iterator(outputDir)){
      const auto name=entry.path().filename().string();
      if(name.ends_with(".jpg"))files.push_back(entry.path());
    }
    std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){
      return a.filename().string()<b.filename().string();});
    Json blocks=Json::array();
    if(!files.empty())requireReadBinding(static_cast<bool>(services.image.maybeResizeAndDownsampleImageBuffer),
      "maybeResizeAndDownsampleImageBuffer");
    for(const auto& file:files){
      const auto raw=filesystem::readFileBytes(file.string());
      const auto resized=services.image.maybeResizeAndDownsampleImageBuffer(raw,raw.size(),"jpeg");
      blocks.push_back(Json{{"type","image"},{"source",{{"type","base64"},
        {"media_type","image/"+resized.mediaType},{"data",base64(resized.buffer)}}}});
    }
    GadgetResult result{data};
    if(!blocks.empty())result.newMessages=std::vector<Json>{createMetaReadMessage(blocks,services)};
    return result;
  }
  requireReadBinding(static_cast<bool>(services.getPDFPageCount),"getPDFPageCount");
  const auto count=services.getPDFPageCount(args.resolvedPath);
  if(count && *count>services.pdfInlineThreshold)
    throw std::runtime_error("This PDF has "+std::to_string(*count)+" pages, which is too many to read at once. "+
      "Use the pages parameter to read specific page ranges (e.g., pages: \"1-5\"). Maximum "+
      std::to_string(services.pdfMaxPagesPerRead)+" pages per request.");
  const auto size=std::filesystem::file_size(args.resolvedPath);
  requireReadBinding(static_cast<bool>(services.isPDFSupported),"isPDFSupported");
  const bool supported=services.isPDFSupported();
  if(!supported||size>services.pdfExtractSizeThreshold){
    requireReadBinding(static_cast<bool>(services.extractPDFPages),"extractPDFPages");
    (void)services.extractPDFPages(args.resolvedPath,std::nullopt);
  }
  if(!supported)throw std::runtime_error(
    "Reading full PDFs is not supported with this model. Use a model with PDF document support, or use the pages parameter to read specific page ranges (e.g., pages: \"1-5\", maximum "+
    std::to_string(services.pdfMaxPagesPerRead)+" pages per request). Page extraction requires poppler-utils: install it for your operating system.");
  requireReadBinding(static_cast<bool>(services.readPDF),"readPDF");
  const auto read=services.readPDF(args.resolvedPath);
  if(!read.at("success").get<bool>())throw std::runtime_error(read.at("error").at("message").get<std::string>());
  const auto data=read.at("data");
  const Json blocks=Json::array({Json{{"type","document"},{"source",{{"type","base64"},
    {"media_type","application/pdf"},{"data",data.at("file").at("base64")}}}}});
  GadgetResult result{data};
  result.newMessages=std::vector<Json>{createMetaReadMessage(blocks,services)};
  return result;
}
} // namespace batbox::gadgets::files
