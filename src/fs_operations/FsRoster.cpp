#include "fs_operations/FsRoster.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::fs_operations {
FsRoster::FsRoster(std::shared_ptr<FsImplementation> original):
    original_(std::move(original)),active_(original_){}
void FsRoster::setFsImplementation(
    std::shared_ptr<FsImplementation> implementation){
  active_=std::move(implementation);
}
std::shared_ptr<FsImplementation> FsRoster::getFsImplementation()const{
  return active_;
}
void FsRoster::setOriginalFsImplementation(){active_=original_;}
std::shared_ptr<WritableStream> createWriteStream(
    const FsImplementation& implementation,const std::string& path){
  if(!implementation.createWriteStream)
    throw std::invalid_argument("createWriteStream import is required");
  return implementation.createWriteStream(path);
}
}
