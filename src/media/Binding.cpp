#include "media/Media.hpp"
#include "formatting/Format.hpp"
#include <stdexcept>

namespace batbox::media {
void bindIndigenousReadMedia(gadgets::files::FileReadServices& services,const Config& config){
  if(!config.getGadgetResultsDir||!config.randomUUID||!config.isPDFSupported)
    throw std::invalid_argument("Missing native media storage, UUID or model PDF support binding");
  services.image.detectImageFormatFromBuffer=detectImageFormatFromBuffer;
  services.image.maybeResizeAndDownsampleImageBuffer=maybeResizeAndDownsampleImageBuffer;
  services.image.compressImageBufferWithTokenLimit=compressImageBufferWithTokenLimit;
  services.image.fallbackJpegResize=fallbackJpegResize;
  services.image.createImageMetadataText=[](const Json& dims){
    return createImageMetadataText(dims).value_or(std::string());};
  services.readPDF=readPDF;
  services.getPDFPageCount=getPDFPageCount;
  services.extractPDFPages=[config](const std::string& path,
    const std::optional<gadgets::files::PdfPageRange>& pages){return extractPDFPages(path,pages,config);};
  services.isPDFSupported=config.isPDFSupported;
  services.formatFileSize=formatting::formatFileSize;
}
} // namespace batbox::media
