#pragma once
#include "attachment_triggers/Attachments.hpp"
#include "attachments/files/Mentions.hpp"
#include "filesystem/FileOps.hpp"
#include "gadgets/files/FileRead.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::attachments::files {
struct Stat {double size=0;bool isDirectory=false;};
struct ReadOptions {std::optional<double> offset,limit;};
enum class Mode {Compact,AtMention};
struct Services {
  event_loop::Loop& loop;
  explicit Services(event_loop::Loop& owner):loop(owner){}
  std::function<app_state::AppStateRef()> getAppState;
  std::function<bool(const std::string&,const app_state::AppStateRef&)>
    isFileReadDenied;
  std::function<std::string(const std::string&)> expandPath,displayPath;
  std::function<bool(const std::string&,std::size_t)> withinReadSizeLimit;
  std::function<event_loop::Deferred<Stat>(const std::string&)> stat;
  std::function<event_loop::Deferred<std::vector<std::string>>(
    const std::string&)> readDirectory;
  std::function<event_loop::Deferred<std::optional<int>>(
    const std::string&)> pdfPageCount;
  std::function<event_loop::Deferred<double>(const std::string&)> modificationTime;
  std::function<event_loop::Deferred<gadgets::ValidationResult>(
    const Json&,execution::Context&)> validateRead;
  std::function<event_loop::Deferred<gadgets::GadgetResult>(
    const Json&,execution::Context&)> callRead;
  std::function<std::optional<std::string>(const Json&)> connectedIdeName;
  std::function<event_loop::Deferred<Json>(const std::string&,
    execution::Context&,const app_state::AppStateRef&)> nestedForFile;
  std::shared_ptr<attachment_triggers::Services> nestedMemory;
  std::size_t maxReadSizeBytes=262144;
  std::size_t maxLinesToRead=2000;
  int pdfInlineThreshold=10;
};

event_loop::Deferred<std::optional<Json>> tryGetPDFReference(
  std::string filename,const Services& services);
event_loop::Deferred<std::optional<Json>> generateFileAttachment(
  std::string filename,execution::Context& context,Mode mode,
  ReadOptions options,const Services& services);
event_loop::Deferred<Json> processAtMentionedFiles(
  const std::string& input,execution::Context& context,const Services& services);
event_loop::Deferred<Json> getSelectedLinesFromIDE(
  const Json& selection,execution::Context& context,const Services& services);
event_loop::Deferred<Json> getOpenedFileFromIDE(
  const Json& selection,execution::Context& context,const Services& services);

// Bind real local filesystem and the indigenous Read gadget. Permission and IDE
// services remain explicit application imports because they read live state.
Services makeIndigenousFileServices(event_loop::Loop& loop,
  gadgets::files::FileReadServices& readServices,
  std::function<app_state::AppStateRef()> getAppState,
  std::function<bool(const std::string&,const app_state::AppStateRef&)>
    isFileReadDenied);
void bindIndigenousNestedMemory(Services& services,
  std::shared_ptr<attachment_triggers::Services> nestedMemory);
} // namespace batbox::attachments::files
