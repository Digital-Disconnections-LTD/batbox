#pragma once
#include "betas/Betas.hpp"

namespace batbox::betas {
// Provider-specific wire values. Backend composition chooses whether
// these headers apply; core beta selection receives Headers as data.
Headers makeProviderWireHeaders(bool connectorTextEnabled,
  bool internalUser);
}
