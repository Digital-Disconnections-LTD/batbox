#pragma once
#include "gadgets/files/FileRead.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::media {
struct Config {
  std::function<std::string()> getGadgetResultsDir;
  std::function<std::string()> randomUUID;
  std::function<bool()> isPDFSupported;
};
std::string detectImageFormatFromBuffer(const std::string& buffer);
std::optional<std::string> createImageMetadataText(const Json& dims,
  const std::optional<std::string>& sourcePath=std::nullopt);
gadgets::files::ImageTransform maybeResizeAndDownsampleImageBuffer(
  const std::string& buffer,std::size_t originalSize,const std::string& ext);
gadgets::files::ImageCompressed compressImageBuffer(
  const std::string& buffer,std::size_t maxBytes=5*1024*1024*3/4,
  const std::string& originalMediaType="");
gadgets::files::ImageCompressed compressImageBufferWithTokenLimit(
  const std::string& buffer,std::size_t maxTokens,const std::string& originalMediaType="");
std::string fallbackJpegResize(const std::string& buffer);
Json readPDF(const std::string& filePath);
std::optional<int> getPDFPageCount(const std::string& filePath);
bool isPdftoppmAvailable();
void resetPdftoppmStash();
Json extractPDFPages(const std::string& filePath,
  const std::optional<gadgets::files::PdfPageRange>& pages,const Config& config);
void bindIndigenousReadMedia(gadgets::files::FileReadServices& services,const Config& config);
} // namespace batbox::media
