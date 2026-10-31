#pragma once
#include "gadgets/files/FileRead.hpp"
#include "gadgets/files/FileEdit.hpp"
#include "gadgets/files/FileWrite.hpp"
#include "gadgets/files/Glob.hpp"
#include "gadgets/files/Grep.hpp"

namespace batbox::gadgets::files {
struct FileGadgetBindings {
  std::function<std::future<Json>(const Json&,const Json&)> checkPermissions;
  std::function<bool(const std::string&,const std::string&)> matchWildcardPattern;
  std::function<std::string(const Json&)> userFacingName;
  std::function<Json(const Json&)> getGadgetUseSummary;
  std::function<Json(const Json&,const Json&)> renderUse;
  std::function<Json(const Json&,const Json&,const Json&)> renderResult;
  std::function<Json(const Json&,const Json&)> renderError;
  std::function<Json(const Json&,const Json&)> renderRejected;
  std::function<Json(const Json&)> renderTag;
  std::function<bool(const Json&)> isResultTruncated;
  std::string batGadgetName="Agent";
  std::string bashGadgetName="Bash";
};
Gadget makeReadGadget(FileReadServices services,FileGadgetBindings bindings);
Gadget makeWriteGadget(FileWriteServices services,FileGadgetBindings bindings);
Gadget makeEditGadget(FileEditServices services,FileGadgetBindings bindings);
Gadget makeGlobGadget(GlobServices services,FileGadgetBindings bindings);
Gadget makeGrepGadget(GrepServices services,FileGadgetBindings bindings);
} // namespace batbox::gadgets::files
