#pragma once
#include "gadgets/files/FileRead.hpp"
#include <stdexcept>

namespace batbox::gadgets::files {
struct ReadArguments {
  std::string inputPath;
  std::string fullPath;
  std::string resolvedPath;
  std::string extension;
  std::size_t offset=1;
  std::optional<std::size_t> limit;
  std::optional<std::string> pages;
  std::size_t maxSizeBytes=262144;
  std::size_t maxTokens=25000;
};
GadgetResult callReadInner(const ReadArguments& args,const GadgetCallContext& context,FileReadServices& services);
GadgetResult callReadPDF(const ReadArguments& args,const GadgetCallContext& context,FileReadServices& services);
void validateReadTokens(const std::string& content,const std::string& extension,
                        std::size_t maxTokens,const FileReadServices& services);
Json createMetaReadMessage(const Json& content,FileReadServices& services);
inline void requireReadBinding(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Missing FileRead binding: ")+name);
}
} // namespace batbox::gadgets::files
