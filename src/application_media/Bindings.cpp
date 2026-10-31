#include "application_media/Bindings.hpp"
#include "media/Base64.hpp"
#include "media/Media.hpp"
#include "media/ImageIndigenous.hpp"
#include "application_media/ClipboardImage.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <fstream>
#include <cctype>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace batbox::application_media {
namespace {
using event_loop::Deferred;
using event_loop::Unit;
template<class Operation> auto worker(event_loop::Loop& owner,Operation operation){
  using Value=decltype(operation());
  Deferred<Value> result(owner);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(owner.hold());
  auto dispatcher=owner.dispatcher();
  try{
    runtime::contextThread([operation=std::move(operation),result,keep,dispatcher]()mutable{
      std::shared_ptr<Value> value;std::exception_ptr error;
      try{value=std::make_shared<Value>(operation());}
      catch(...){error=std::current_exception();}
      try{dispatcher.postMission([result,keep,value,error]{
        if(error)result.reject(error);else result.resolve(std::move(*value));
        keep->release();});}
      catch(...){keep->release();}
    }).detach();
  }catch(...){keep->release();result.reject(std::current_exception());}
  return result;
}
int imageId(const std::string& key){
  int id=-1;
  const auto [end,error]=std::from_chars(key.data(),key.data()+key.size(),id);
  if(error!=std::errc{}||end!=key.data()+key.size()||id<0)
    throw std::invalid_argument("Invalid pasted image ID");
  return id;
}
std::string extension(const Json& content){
  const auto& mediaType=value::property(content,"mediaType");
  if(!mediaType.is_string())return "png";
  const auto value=mediaType.get<std::string>();
  if(value=="image/jpeg")return "jpeg";
  if(value=="image/gif")return "gif";
  if(value=="image/webp")return "webp";
  if(value=="image/bmp")return "bmp";
  return "png";
}
void writeImage(const std::filesystem::path& path,const std::string& bytes){
#ifdef _WIN32
  const int fd=_open(path.string().c_str(),_O_WRONLY|_O_CREAT|_O_TRUNC|_O_BINARY,
    _S_IREAD|_S_IWRITE);
#else
  const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
#endif
  if(fd<0)throw std::runtime_error("Could not open pasted image cache file");
  try{
    std::size_t offset=0;
    while(offset<bytes.size()){
#ifdef _WIN32
      const auto written=_write(fd,bytes.data()+offset,
        static_cast<unsigned>(std::min<std::size_t>(bytes.size()-offset,1<<20)));
#else
      const auto written=::write(fd,bytes.data()+offset,bytes.size()-offset);
#endif
      if(written<0&&errno==EINTR)continue;
      if(written<=0)throw std::runtime_error("Could not write pasted image cache file");
      offset+=static_cast<std::size_t>(written);
    }
#ifdef _WIN32
    if(_commit(fd)!=0)throw std::runtime_error("Could not sync pasted image cache file");
    _close(fd);
#else
#ifdef __APPLE__
    if(::fsync(fd)!=0)throw std::runtime_error("Could not sync pasted image cache file");
#else
    if(::fdatasync(fd)!=0)throw std::runtime_error("Could not sync pasted image cache file");
#endif
    ::close(fd);
#endif
  }catch(...){
#ifdef _WIN32
    _close(fd);
#else
    ::close(fd);
#endif
    throw;
  }
}
std::string trim(std::string value){
  const auto first=value.find_first_not_of(" \t\r\n");
  if(first==std::string::npos)return {};
  const auto last=value.find_last_not_of(" \t\r\n");
  return value.substr(first,last-first+1);
}
std::string normalizedImagePath(std::string value){
  value=trim(std::move(value));
  if(value.size()>1&&((value.front()=='\"'&&value.back()=='\"')||
      (value.front()=='\''&&value.back()=='\'')))
    value=value.substr(1,value.size()-2);
#ifndef _WIN32
  std::string unescaped;unescaped.reserve(value.size());
  for(std::size_t i=0;i<value.size();++i){
    if(value[i]=='\\'&&i+1<value.size()){
      if(value[i+1]=='\\'){unescaped+='\\';++i;continue;}
      unescaped+=value[++i];continue;
    }
    unescaped+=value[i];
  }
  return unescaped;
#else
  return value;
#endif
}
bool imageExtension(const std::string& path){
  auto extension=std::filesystem::path(path).extension().string();
  std::transform(extension.begin(),extension.end(),extension.begin(),
    [](unsigned char value){return static_cast<char>(std::tolower(value));});
  return extension==".png"||extension==".jpg"||extension==".jpeg"||
    extension==".gif"||extension==".webp";
}
std::vector<std::string> pasteParts(const std::string& text){
  // Split at newline and before another absolute path separated by a
  // space. Escaped spaces within a path remain part of the same candidate.
  std::vector<std::string> parts;std::string part;
  for(std::size_t i=0;i<text.size();++i){
    const bool nextPath=text[i]==' '&&i+1<text.size()&&
      (text[i+1]=='/'||(i+3<text.size()&&
        std::isalpha(static_cast<unsigned char>(text[i+1]))&&
        text[i+2]==':'&&text[i+3]=='\\'));
    if(text[i]=='\n'||nextPath){
      if(!trim(part).empty())parts.push_back(part);
      part.clear();continue;
    }
    part+=text[i];
  }
  if(!trim(part).empty())parts.push_back(part);
  return parts;
}
std::optional<Json> readImagePath(const std::string& source,
    const std::optional<std::string>& clipboardPath){
  try{
    const auto path=normalizedImagePath(source);
    if(!imageExtension(path))return {};
    std::filesystem::path file(path);
    if(!file.is_absolute()){
      if(!clipboardPath||!std::filesystem::path(*clipboardPath).is_absolute()||
          file!=std::filesystem::path(*clipboardPath).filename())return {};
      file=*clipboardPath;
    }
    std::error_code error;
    if(!std::filesystem::is_regular_file(file,error)||error||
       std::filesystem::file_size(file,error)>32*1024*1024||error)return {};
    std::ifstream stream(file,std::ios::binary);
    if(!stream)return {};
    const std::string bytes(std::istreambuf_iterator<char>{stream},{});
    if(bytes.empty()||bytes.size()>32*1024*1024)return {};
    const auto extension=file.extension().string();
    const auto resized=media::maybeResizeAndDownsampleImageBuffer(
      bytes,bytes.size(),extension.substr(1));
    const auto mime=media::detectImageFormatFromBuffer(resized.buffer);
    if(!mime.starts_with("image/"))return {};
    return Json{{"base64",media::indigenous::base64Encode(resized.buffer)},
      {"mediaType",mime},{"dimensions",resized.dimensions},
      {"sourcePath",file.string()},{"filename",file.filename().string()}};
  }catch(...){return {};}
}
}

Bundle::Bundle(Inputs inputs):inputs_(std::move(inputs)),clipboard_(inputs_.owner,
    termio::indigenousClipboardServices(inputs_.owner,termio::indigenousTerminalEnvironment(),
      inputs_.terminalName?inputs_.terminalName:[]{return std::string{};},
      process::ExecImports{})){}
std::shared_ptr<Bundle> Bundle::create(Inputs inputs){
  if(!inputs.owner.isOwnerThread()||!inputs.flightId)
    throw std::invalid_argument("Media owner and session ID required");
  return std::shared_ptr<Bundle>(new Bundle(std::move(inputs)));
}
std::filesystem::path Bundle::imageStoreDirectory()const{
  return inputs_.stateDirectory/"image-cache"/inputs_.flightId();
}
Deferred<Json> Bundle::storeImages(const Json& pastedContents){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Image store owner required");
  Deferred<Json> result(inputs_.owner);
  if(!pastedContents.is_object()||pastedContents.empty()){
    result.resolve(Json::object());return result;
  }
  struct Record {int id;std::string extension,bytes;};
  std::vector<Record> records;
  try{
    for(auto it=pastedContents.begin();it!=pastedContents.end();++it){
      const auto& content=it.value();
      if(value::property(content,"type")!="image")continue;
      const auto& data=value::property(content,"content");
      if(!data.is_string())continue;
      records.push_back({imageId(it.key()),extension(content),
        media::indigenous::base64Decode(data.get<std::string>())});
    }
    std::sort(records.begin(),records.end(),[](const Record& a,const Record& b){
      return a.id<b.id;});
    auto pending=worker(inputs_.owner,[records=std::move(records),
        dir=imageStoreDirectory()]()mutable{
      Json paths=Json::object();
      std::error_code error;std::filesystem::create_directories(dir,error);
      if(error)return paths;
      for(const auto& record:records){
        const auto path=dir/(std::to_string(record.id)+"."+record.extension);
        try{writeImage(path,record.bytes);paths[std::to_string(record.id)]=path.string();}
        catch(...){/* Continue processing the remaining images after a write failure. */}
      }
      return paths;
    });
    auto self=shared_from_this();
    pending.then([self,result](const Json* paths,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      if(!paths){result.reject(std::make_exception_ptr(
        std::logic_error("Stored image paths missing")));return;}
      for(auto it=paths->begin();it!=paths->end();++it){
        const int id=imageId(it.key());
        self->pathOrder_.erase(std::remove(self->pathOrder_.begin(),
          self->pathOrder_.end(),id),self->pathOrder_.end());
        while(self->pathOrder_.size()>=200){
          self->storedPaths_.erase(self->pathOrder_.front());self->pathOrder_.pop_front();}
        self->pathOrder_.push_back(id);self->storedPaths_[id]=it.value().get<std::string>();
      }
      result.resolve(*paths);
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
Deferred<Json> Bundle::resizeImageBlock(const Json& block){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Image resize owner required");
  Deferred<Json> output(inputs_.owner);
  const auto& source=value::property(block,"source");
  if(value::property(source,"type")!="base64"){
    output.resolve(Json{{"block",block}});return output;
  }
  const auto& data=value::property(source,"data");
  if(!data.is_string()){
    output.reject(std::make_exception_ptr(std::invalid_argument("Image data required")));
    return output;
  }
  auto pending=worker(inputs_.owner,[encoded=data.get<std::string>(),source]{
    const auto bytes=media::indigenous::base64Decode(encoded);
    const auto& mime=value::property(source,"media_type");
    const std::string extension=mime.is_string()&&mime.get<std::string>().starts_with("image/")
      ?mime.get<std::string>().substr(6):"png";
    const auto resized=media::maybeResizeAndDownsampleImageBuffer(bytes,bytes.size(),extension);
    return Json{{"block",Json{{"type","image"},{"source",Json{
      {"type","base64"},{"media_type","image/"+resized.mediaType},
      {"data",media::indigenous::base64Encode(resized.buffer)}}}}},
      {"dimensions",resized.dimensions}};
  });
  pending.then([output](const Json* value,const std::exception_ptr& error){
    if(error)output.reject(error);else output.resolve(*value);
  });
  return output;
}
std::optional<std::string> Bundle::imageMetadata(const Json& dimensions,
    const std::optional<std::string>& sourcePath)const{
  return media::createImageMetadataText(dimensions,sourcePath);
}
std::optional<std::string> Bundle::storedImagePath(int id)const{
  if(const auto it=storedPaths_.find(id);it!=storedPaths_.end())return it->second;
  return std::nullopt;
}
void Bundle::acceptPastedImage(const Json& content){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Image paste owner required");
  const auto& rawId=value::property(content,"id");
  if(value::property(content,"type")!="image"||!rawId.is_number_integer())return;
  const int id=rawId.get<int>();if(id<0)return;
  const auto path=(imageStoreDirectory()/
    (std::to_string(id)+"."+extension(content))).string();
  pathOrder_.erase(std::remove(pathOrder_.begin(),pathOrder_.end(),id),pathOrder_.end());
  while(pathOrder_.size()>=200){
    storedPaths_.erase(pathOrder_.front());pathOrder_.pop_front();}
  pathOrder_.push_back(id);storedPaths_[id]=path;
  auto self=shared_from_this();
  Json images=Json::object();images[std::to_string(id)]=content;
  storeImages(images).then([self,id](const Json* result,const std::exception_ptr& error){
    if(error&&self->inputs_.onError)self->inputs_.onError(error);
    else if(result&&!result->contains(std::to_string(id))&&self->inputs_.log)
      self->inputs_.log("Pasted image cache write failed");
  });
}
void Bundle::clearStoredImagePaths(){storedPaths_.clear();pathOrder_.clear();}
Deferred<Json> Bundle::getImageFromClipboard(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Clipboard image owner required");
  return worker(inputs_.owner,[]{return indigenous::getImageFromClipboard();});
}
Deferred<Json> Bundle::extractPastedImages(const std::string& text){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Image paste owner required");
  return worker(inputs_.owner,[text]{
    Json images=Json::array();std::vector<std::string> other;
    const auto parts=pasteParts(text);
    const bool hasCandidate=std::any_of(parts.begin(),parts.end(),[](const auto& part){
      return imageExtension(normalizedImagePath(part));});
    if(!hasCandidate)
      return Json{{"images",std::move(images)},{"text",text}};
    std::optional<std::string> clipboardPath;
    if(std::any_of(parts.begin(),parts.end(),[](const auto& part){
          const auto path=normalizedImagePath(part);
          return imageExtension(path)&&
            !std::filesystem::path(path).is_absolute();}))
      clipboardPath=indigenous::getImagePathFromClipboard();
    for(const auto& part:parts){
      if(imageExtension(normalizedImagePath(part))){
        if(const auto image=readImagePath(part,clipboardPath))
          images.push_back(*image);
      }else other.push_back(part);
    }
    if(images.empty()){
#ifdef __APPLE__
      std::string lower=text;
      std::transform(lower.begin(),lower.end(),lower.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
      if(lower.find("/temporaryitems/")!=std::string::npos&&
          lower.find("screencaptureui")!=std::string::npos&&
          lower.find("/screenshot")!=std::string::npos){
        auto clipboard=indigenous::getImageFromClipboard();
        if(clipboard.is_object())images.push_back(std::move(clipboard));
        return Json{{"images",std::move(images)},{"text",""}};
      }
#endif
      return Json{{"images",std::move(images)},{"text",text}};
    }
    std::string remaining;
    for(const auto& part:other){if(!remaining.empty())remaining+='\n';remaining+=part;}
    return Json{{"images",std::move(images)},{"text",std::move(remaining)}};
  });
}
Deferred<Unit> Bundle::cleanupOldImageStashes(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Image cleanup owner required");
  return worker(inputs_.owner,[base=inputs_.stateDirectory/"image-cache",
      current=inputs_.flightId()]{
    std::error_code error;
    for(std::filesystem::directory_iterator it(base,error),end;!error&&it!=end;
        it.increment(error)){
      if(it->is_directory(error)&&it->path().filename()!=current)
        std::filesystem::remove_all(it->path(),error);
      error.clear();
    }
    return Unit{};
  });
}
void Bundle::copyMessageText(const std::string& text,
    std::function<void(const std::string&)> writeTerminal,
    std::function<void(const interactive::navigation::CopyNotification&)> notify){
  interactive::navigation::CopyServices services(inputs_.owner);
  interactive::navigation::bindClipboard(services,clipboard_);
  services.writeTerminal=std::move(writeTerminal);
  services.addNotification=std::move(notify);
  services.onDetachedError=inputs_.onError;
  interactive::navigation::copyMessageText(text,services);
}
} // namespace batbox::application_media
