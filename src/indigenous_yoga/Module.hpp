#pragma once
#include "event_loop/Deferred.hpp"
#include "indigenous_yoga/Node.hpp"

namespace batbox::indigenous_yoga {
struct ConfigFactory {
  ConfigRef create()const{return createConfig();}
  void destroy(const ConfigRef&)const{}
};
struct NodeFactory {
  NodeRef create(ConfigRef config={})const{
    return std::make_shared<Node>(std::move(config));
  }
  NodeRef createDefault()const{return std::make_shared<Node>();}
  NodeRef createWithConfig(ConfigRef config)const{
    return std::make_shared<Node>(std::move(config));
  }
  void destroy(const NodeRef&)const{}
};
struct YogaModule {ConfigFactory Config;NodeFactory Node;};
using YogaModuleRef=std::shared_ptr<YogaModule>;
YogaModuleRef yogaInstance();
event_loop::Deferred<YogaModuleRef> loadYoga(event_loop::Loop& loop);
} // namespace batbox::indigenous_yoga
