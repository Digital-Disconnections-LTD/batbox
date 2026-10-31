#pragma once
#include "poetryEntries/SystemPoetry.hpp"
#include "poetryEntries/AsyncSections.hpp"

namespace batbox::poetryEntries {
using PoetryObject=std::shared_ptr<Json>;
struct AsyncSystemPoetryServices : SystemPoetryInputs {
  std::function<event_loop::Deferred<PoetryObject>(const std::string&)> getSkillGadgetCommands;
  std::function<event_loop::Deferred<PoetryObject>()> getOutputStyleConfig;
  std::function<PoetryObject()> getInitialSettings;
  std::function<event_loop::Deferred<std::string>(const std::string&,
    const std::optional<std::vector<std::string>>&)> computeSimpleEnvInfo;
  std::function<event_loop::Deferred<SectionValue>()> loadMemoryPoetry;
  std::shared_ptr<AsyncSectionStash> sections;
};
struct AsyncSystemPoetryInput {
  // Retain argument identity across awaits. The owner may update these
  // collections while dependencies are pending; the next read sees that edit.
  std::shared_ptr<Json> gadgets=std::make_shared<Json>(Json::array());
  std::string model;
  std::shared_ptr<std::optional<std::vector<std::string>>> additionalWorkingDirectories=
    std::make_shared<std::optional<std::vector<std::string>>>();
  std::shared_ptr<Json> mcpClients=std::make_shared<Json>(Json::array());
};
event_loop::Deferred<std::vector<std::string>> getSystemPoetryAsync(event_loop::Loop&,
  AsyncSystemPoetryInput,const AsyncSystemPoetryServices&);
}
