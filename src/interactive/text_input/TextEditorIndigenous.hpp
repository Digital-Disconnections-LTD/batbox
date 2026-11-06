#pragma once

#include "interactive/text_input/TextEditor.hpp"
#include <memory>

namespace batbox::interactive::text_input {

// Supplies the concrete clock and ANSI-strip dependency. Application callbacks
// such as history, notifications and submission stay bound to their owners.
EditorCallbacks indigenousEditorCallbacks(EditorCallbacks callbacks);

} // namespace batbox::interactive::text_input
