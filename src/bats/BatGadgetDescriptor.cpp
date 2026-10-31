#include "bats/BatGadgetDescriptor.hpp"

namespace batbox::bats {
void configureBatGadgetDescriptor(gadgets::Gadget& gadget,
    const BatGadgetDescriptorImports& imports){
  gadget.name="Agent";
  gadget.aliases={"Task"};
  gadget.searchHint="delegate work to a Bat";
  gadget.maxResultSizeChars=100000;
  gadget.description=[](const Json&,const Json&){
    std::promise<std::string> ready;
    ready.set_value("Launch a new Bat");
    return ready.get_future();
  };
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{
    return batGadgetAutoClassifierInput(input);
  };
  gadget.getActivityDescription=[](const Json& input){
    return batGadgetActivityDescription(input);
  };
  gadget.checkPermissions=[](const Json& input,const Json&){
    std::promise<Json> ready;
    ready.set_value(Json{{"behavior","allow"},{"updatedInput",input}});
    return ready.get_future();
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[read=imports.fileReadGadgetName,
      shell=imports.bashGadgetName](const Json& data,const std::string& gadgetUseID){
    return mapBatGadgetResult(data,gadgetUseID,read,shell);
  };
  gadget.userFacingName=imports.userFacingName;
  gadget.userFacingNameBackgroundColor=imports.userFacingNameBackgroundColor;
  gadget.renderGadgetUseMessage=imports.renderGadgetUseMessage;
  gadget.renderGadgetResultMessage=imports.renderGadgetResultMessage;
  gadget.renderGadgetUseProgressMessage=imports.renderGadgetUseProgressMessage;
  gadget.renderGadgetUseRejectedMessage=imports.renderGadgetUseRejectedMessage;
  gadget.renderGadgetUseErrorMessage=imports.renderGadgetUseErrorMessage;
  gadget.renderGadgetUseTag=imports.renderGadgetUseTag;
  gadget.renderGroupedGadgetUse=imports.renderGroupedGadgetUse;
}
} // namespace batbox::bats
