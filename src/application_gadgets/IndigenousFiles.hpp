#pragma once
#include "application_gadgets/ApplicationGadgets.hpp"

namespace batbox::application_gadgets {
struct IndigenousFileServices {
  gadgets::files::FileReadServices read;
  gadgets::files::FileWriteServices write;
  gadgets::files::FileEditServices edit;
  gadgets::files::GlobServices glob;
  gadgets::files::GrepServices grep;
};
IndigenousFileServices makeIndigenousFileServices(const Inputs& input);
} // namespace batbox::application_gadgets
