#pragma once
#include "settings/Parse.hpp"
#include "settings/Paths.hpp"

namespace batbox::settings {
struct ManagedPresence {bool hasBase=false,hasDropIns=false;};
ParsedSettings loadManagedFileSettings(Paths& paths,Stash& stash,
  const ParseServices& services);
ManagedPresence getManagedFileSettingsPresence(Paths& paths,Stash& stash,
  const ParseServices& services);
}
