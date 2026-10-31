#include "media/Media.hpp"
#include "media/ImageIndigenous.hpp"
#include "filesystem/FileOps.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <utility>
#include <vector>
using batbox::Json;
namespace media=batbox::media;
int main(int argc,char** argv){
  assert(argc==5);
  const std::string smallPath=argv[1],largePath=argv[2],pdfPath=argv[3],outdir=argv[4];
  const auto small=batbox::filesystem::readFileBytes(smallPath);
  const auto large=batbox::filesystem::readFileBytes(largePath);
  assert(media::detectImageFormatFromBuffer(small)=="image/png");
  assert(media::detectImageFormatFromBuffer(large)=="image/png");
  auto unchanged=media::maybeResizeAndDownsampleImageBuffer(small,small.size(),"png");
  assert(unchanged.buffer==small&&unchanged.mediaType=="png");
  assert(unchanged.dimensions["originalWidth"]==10&&unchanged.dimensions["displayWidth"]==10);
  auto resized=media::maybeResizeAndDownsampleImageBuffer(large,large.size(),"png");
  auto metadata=media::indigenous::inspect(resized.buffer);
  assert(resized.dimensions["originalWidth"]==3000&&resized.dimensions["originalHeight"]==1500);
  assert(resized.dimensions["displayWidth"]==2000&&resized.dimensions["displayHeight"]==1000);
  assert(metadata.width<=2000&&metadata.height<=1000);
  auto compressed=media::compressImageBufferWithTokenLimit(large,1000,"image/png");
  assert(compressed.mediaType.starts_with("image/")&&compressed.originalSize==large.size());
  auto fallback=media::fallbackJpegResize(large);
  assert(media::detectImageFormatFromBuffer(fallback)=="image/jpeg");
  assert(media::indigenous::inspect(fallback).width<=400);
  for(const auto& [format,mime]:std::vector<std::pair<std::string,std::string>>{
    {"jpeg","image/jpeg"},{"webp","image/webp"},{"gif","image/gif"}}){
    const auto encoded=media::indigenous::encode(small,0,0,format);
    assert(media::detectImageFormatFromBuffer(encoded)==mime);
    assert(media::indigenous::inspect(encoded).width==10);
    const auto preserved=media::maybeResizeAndDownsampleImageBuffer(encoded,encoded.size(),format);
    assert(preserved.buffer==encoded&&preserved.mediaType==format);
  }
  const auto largeWebp=media::indigenous::encode(large,0,0,"webp");
  const auto smallerWebp=media::maybeResizeAndDownsampleImageBuffer(
    largeWebp,largeWebp.size(),"webp");
  assert(smallerWebp.mediaType=="webp");
  assert(media::detectImageFormatFromBuffer(smallerWebp.buffer)=="image/webp");
  assert(media::indigenous::inspect(smallerWebp.buffer).width<=2000);
  auto note=media::createImageMetadataText(resized.dimensions);
  assert(note&&note->find("Multiply coordinates by 1.50")!=std::string::npos);
  assert(!media::createImageMetadataText(unchanged.dimensions));
  auto pdf=media::readPDF(pdfPath);
  assert(pdf["success"]==true&&pdf["data"]["type"]=="pdf");
  assert(media::getPDFPageCount(pdfPath)==2);
  assert(media::isPdftoppmAvailable());
  media::resetPdftoppmStash();
  assert(media::isPdftoppmAvailable());
  media::Config config;
  config.getGadgetResultsDir=[&](){return outdir;};
  config.randomUUID=[](){return std::string("fixture");};
  config.isPDFSupported=[](){return true;};
  auto pages=media::extractPDFPages(pdfPath,batbox::gadgets::files::PdfPageRange{1,2},config);
  assert(pages["success"]==true&&pages["data"]["file"]["count"]==2);
  auto full=media::extractPDFPages(pdfPath,std::nullopt,config);
  assert(full["success"]==true&&full["data"]["file"]["count"]==2);
  batbox::gadgets::files::FileReadServices services;
  media::bindIndigenousReadMedia(services,config);
  assert(services.image.detectImageFormatFromBuffer&&services.readPDF&&services.extractPDFPages);
  const auto readImage=batbox::gadgets::files::readImageWithTokenBudget(
    largePath,100000,std::nullopt,services.image);
  assert(readImage["type"]=="image"&&readImage["file"]["type"]=="image/png");
  assert(readImage["file"]["dimensions"]["displayWidth"]==2000);
  assert(services.readPDF(pdfPath)["success"]==true);
  std::cout<<"native image/PDF processing passed\n";
}
