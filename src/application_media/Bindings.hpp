#pragma once
#include "event_loop/Deferred.hpp"
#include "interactive/navigation/MessageCopy.hpp"
#include "termio/Clipboard.hpp"
#include <deque>
#include <filesystem>
#include <map>
#include <memory>

namespace batbox::application_media {
struct Inputs {
  event_loop::Loop& owner;
  std::filesystem::path stateDirectory;
  std::function<std::string()> flightId;
  std::function<void(const std::string&)> log;
  std::function<void(std::exception_ptr)> onError;
  std::function<std::string()> terminalName;
  Inputs(event_loop::Loop& loop):owner(loop){}
};

// Indigenous application boundary for image storage, image-block resizing,
// and terminal clipboard. The image stash is scoped to the current flight ID.
class Bundle final:public std::enable_shared_from_this<Bundle> {
public:
  static std::shared_ptr<Bundle> create(Inputs inputs);
  event_loop::Deferred<Json> storeImages(const Json& pastedContents);
  event_loop::Deferred<Json> resizeImageBlock(const Json& block);
  event_loop::Deferred<Json> getImageFromClipboard();
  // Resolve bracketed-paste image file paths off the owner loop. If all image
  // candidates fail, the original paste is returned as ordinary text.
  event_loop::Deferred<Json> extractPastedImages(const std::string& text);
  std::optional<std::string> imageMetadata(const Json& dimensions,
    const std::optional<std::string>& sourcePath)const;
  std::optional<std::string> storedImagePath(int id)const;
  void acceptPastedImage(const Json& content);
  void clearStoredImagePaths();
  event_loop::Deferred<event_loop::Unit> cleanupOldImageStashes();
  void copyMessageText(const std::string& text,
    std::function<void(const std::string&)> writeTerminal,
    std::function<void(const interactive::navigation::CopyNotification&)> notify);
private:
  explicit Bundle(Inputs inputs);
  std::filesystem::path imageStoreDirectory()const;
  Inputs inputs_;
  termio::ClipboardModule clipboard_;
  std::map<int,std::string> storedPaths_;
  std::deque<int> pathOrder_;
};
} // namespace batbox::application_media
