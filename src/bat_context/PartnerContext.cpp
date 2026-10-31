#include "bat_context/PartnerContext.hpp"

namespace batbox::bat_context {
runtime::AsyncLocalStorage<PartnerContext>& partnerContextStorage(){
  static runtime::AsyncLocalStorage<PartnerContext> storage;
  return storage;
}
PartnerRef getPartnerContext(){return partnerContextStorage().getStore();}
bool isInProcessPartner(){return bool(partnerContextStorage().getStore());}
PartnerRef createPartnerContext(const PartnerSpawnConfig& config){
  auto result=std::make_shared<PartnerContext>();
  static_cast<PartnerSpawnConfig&>(*result)=config;
  result->isInProcess=true;
  return result;
}
}
