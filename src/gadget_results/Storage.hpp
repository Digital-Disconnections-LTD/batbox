#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace batbox::gadget_results {
inline constexpr auto GADGET_RESULTS_SUBDIR="tool-results";
inline constexpr auto PERSISTED_OUTPUT_TAG="<persisted-output>";
inline constexpr auto PERSISTED_OUTPUT_CLOSING_TAG="</persisted-output>";
inline constexpr auto GADGET_RESULT_CLEARED_MESSAGE="[Old tool result content cleared]";
inline constexpr int PREVIEW_SIZE_BYTES=2000;
inline constexpr double DEFAULT_MAX_RESULT_SIZE_CHARS=50000;
inline constexpr double MAX_GADGET_RESULT_BYTES=400000;
inline constexpr double MAX_GADGET_RESULTS_PER_MESSAGE_CHARS=200000;
using Completion=event_loop::Deferred<event_loop::Unit>;
struct FileSystemError:std::runtime_error {
  std::optional<std::string> code,path;
  FileSystemError(std::string message,std::optional<std::string> code={},
    std::optional<std::string> path={}):std::runtime_error(std::move(message)),
    code(std::move(code)),path(std::move(path)){}
};
struct StorageImports {
  std::function<std::string()> getOriginalCwd,getFlightId;
  std::function<std::string(const std::string&)> getProjectDir;
  std::function<Completion(const std::string&)> mkdir;
  std::function<Completion(const std::string&,const std::string&)> writeExclusive;
  std::function<void(std::exception_ptr)> logError;
  std::function<void(const std::string&)> debug;
};
StorageImports indigenousStorageImports(event_loop::Loop&,StorageImports paths);
std::string getFlightDir(const StorageImports&);
std::string getGadgetResultsDir(const StorageImports&);
std::string getGadgetResultPath(const StorageImports&,const std::string& id,bool isJson);
Completion ensureGadgetResultsDir(event_loop::Loop&,const StorageImports&);
event_loop::Deferred<Json> persistGadgetResult(event_loop::Loop&,const StorageImports&,
  const Json& content,const std::string& gadgetUseId);
Json generatePreview(const std::string& content,double maxBytes);
std::string buildLargeGadgetResultMessage(const Json& result);
bool isPersistError(const Json&);
std::string getFileSystemErrorMessage(const FileSystemError&);
double getPersistenceThreshold(const std::string& gadgetName,double declaredMax,const Json& localOverrides);
double getPerMessageBudgetLimit(const Json& localOverride);
bool isGadgetResultContentEmpty(const Json& content);
bool hasImageBlock(const Json& content);
bool isContentAlreadyCompacted(const Json& content);
double contentSize(const Json& content);
event_loop::Deferred<Json> maybePersistLargeGadgetResult(event_loop::Loop&,
  const StorageImports&,const Json& block,const std::string& gadgetName,
  std::optional<double> threshold={});
event_loop::Deferred<Json> processPreMappedGadgetResultBlock(event_loop::Loop&,
  const StorageImports&,const Json& block,const std::string& gadgetName,
  double maxResultSizeChars,const Json& localOverrides=Json::object());
event_loop::Deferred<Json> processGadgetResultBlock(event_loop::Loop&,
  const StorageImports&,const std::string& name,double maxResultSizeChars,
  const std::function<Json(const Json&,const std::string&)>& mapResult,
  const Json& result,const std::string& id,const Json& localOverrides=Json::object());
} // namespace batbox::gadget_results
