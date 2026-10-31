#pragma once
#include "mcp/Mcp.hpp"

namespace batbox::mcp {
struct StorageConfig {
  std::function<void()> ensureGadgetResultsDir;
  std::function<std::string()> getGadgetResultsDir;
  std::function<std::int64_t()> nowMs;
  std::function<std::string()> randomSuffix;
  std::string hostedServerPrefix="batbox.ai ";
};
std::string extensionForMimeType(const std::optional<std::string>& mimeType);
bool isBinaryContentType(const std::string& contentType);
Json persistBinaryContent(const std::string& bytes,const std::optional<std::string>& mimeType,
  const std::string& persistId,const StorageConfig& config);
std::string getBinaryBlobSavedMessage(const std::string& filepath,
  const std::optional<std::string>& mimeType,double size,const std::string& sourceDescription);
std::string getFormatDescription(const std::string& type,const std::string& schema);
std::string getLargeOutputInstructions(const std::string& path,double contentLength,
  const std::string& formatDescription,std::optional<double> maxReadLength=std::nullopt);
void bindIndigenousResultServices(ResultServices& services,const StorageConfig& config);
} // namespace batbox::mcp
