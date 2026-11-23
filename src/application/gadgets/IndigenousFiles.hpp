#pragma once
#include "application/gadgets/ApplicationGadgets.hpp"

namespace batbox::application::gadgets {
struct IndigenousFileServices {
  batbox::gadgets::files::FileReadServices read;
  batbox::gadgets::files::FileWriteServices write;
  batbox::gadgets::files::FileEditServices edit;
  batbox::gadgets::files::GlobServices glob;
  batbox::gadgets::files::GrepServices grep;
};
IndigenousFileServices makeIndigenousFileServices(const Inputs& input);
} // namespace batbox::application::gadgets
