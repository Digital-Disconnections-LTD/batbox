foreach(section IN ITEMS poetry launch async foreground_progress foreground_final background
    foreground_loop call descriptor_poetry race_order zod schema)
  batbox_probe(bats_gadget_${section} batbox_bats phase4)
endforeach()
foreach(section IN ITEMS stream race foreground binding descriptor_poetry schema progress)
  batbox_indigenous(phase4-bats-gadget-${section}-indigenous
    tests/phase4/bats_gadget_${section}_indigenous.cpp batbox_bats)
endforeach()
batbox_indigenous(phase4-bats-resume-typed tests/phase4/bats_resume_typed_indigenous.cpp batbox_bats)
