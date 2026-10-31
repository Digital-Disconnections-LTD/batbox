# Unicode lookup tables

`stage1` selects a Unicode code-point page; `stage2` selects a compressed
grapheme property; `stage3` decodes its class. `wide`, `ambiguous`, and
`emojiProperty` contain width/property intervals.

`Tables.cpp` assembles the arrays and implements lookup functions. Preserve
array sizes, ordering, and the adjacent Unicode and Bun license notices when
updating the data. Exercise the width fixtures after a change.
