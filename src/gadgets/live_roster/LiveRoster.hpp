#pragma once
#include "bats/BatGadgetBinding.hpp"
#include "bats/BatGadgetDescriptor.hpp"
#include "bats/BatGadgetSchema.hpp"
#include "mcp/Mcp.hpp"
#include "gadgets/Catalog.hpp"
#include "gadgets/bash/BashGadget.hpp"
#include "gadgets/files/FileGadgets.hpp"
#include "gadgets/web/Web.hpp"
#include <deque>
#include <mutex>

namespace batbox::gadgets::live_roster {
struct BatFactory {
  bats::BatGadgetDescriptorImports descriptor;
  bats::BatGadgetSchemaOptions schema;
  bats::BatGadgetPoetryDependencies poetry;
  std::function<bats::BatContext(const GadgetCallContext&)> makeContext;
  std::function<bats::BatGadgetCallServices()> makeServices;
};
struct FactoryInputs {
  std::optional<BatFactory> bat;
  // Indigenous BatApplication may own a fully configured descriptor whose
  // services depend on the already-constructed execution engine.
  std::function<Gadget()> makeBatGadget;
  std::optional<bash_gadget::Services> bash;
  std::function<Gadget(bash_gadget::Services)> makeBash;
  std::optional<files::FileReadServices> read;
  std::optional<files::FileWriteServices> write;
  std::optional<files::FileEditServices> edit;
  std::optional<files::GlobServices> glob;
  std::optional<files::GrepServices> grep;
  files::FileGadgetBindings fileBindings;
  std::function<files::FileGadgetBindings(const std::string&)> makeFileBindings;
  std::optional<web::SearchServices> search;
  std::optional<web::FetchServices> fetch;
  std::shared_ptr<Searxng> searxng;
  std::shared_ptr<Scrapling> scrapling;
  std::vector<std::shared_ptr<mcp::Connection>> mcpConnections;
  mcp::Services mcpServices;
  std::vector<Gadget> additionalBaseGadgets;
};

struct Snapshot {
  // App turns retain the Snapshot while using raw Gadget pointers. Refreshing
  // the roster never changes objects held by an older turn.
  std::deque<Gadget> owned;
  Gadgets base,mcp;
  std::shared_ptr<Searxng> searxng;
  std::shared_ptr<Scrapling> scrapling;
  std::vector<std::shared_ptr<mcp::Connection>> mcpConnections;
  const Gadget* find(const std::string& name)const;
};
struct Selection {
  std::shared_ptr<const Snapshot> owner;
  Gadgets gadgets;
};

std::shared_ptr<Snapshot> makeSnapshot(FactoryInputs inputs);
Selection select(const std::shared_ptr<const Snapshot>& snapshot,
  const Json& permissionContext,const CatalogSettings& settings);
Selection selectFull(const std::shared_ptr<const Snapshot>& snapshot,
  const Json& permissionContext,const CatalogSettings& settings,
  const std::function<CatalogImports(const Snapshot&)>& imports);

class Roster {
  mutable std::mutex mutex_;
  std::shared_ptr<const Snapshot> current_;
  // Legacy consumers may export raw pointers. Indigenous contexts carry an owner
  // sidecar; retain replaced snapshots until the app reaches quiescence.
  std::vector<std::shared_ptr<const Snapshot>> retired_;
 public:
  std::shared_ptr<const Snapshot> publish(std::shared_ptr<Snapshot> next);
  std::shared_ptr<const Snapshot> replaceMcp(std::vector<Gadget> gadgets,
    std::vector<std::shared_ptr<mcp::Connection>> connections);
  std::shared_ptr<const Snapshot> current()const;
  void releaseRetiredAtQuiescence();
  std::size_t retiredCount()const;
  Selection selectCurrent(const Json& permissionContext,
    const CatalogSettings& settings)const;
};
} // namespace batbox::gadgets::live_roster
