foreach(section IN ITEMS stream foreground binding descriptor_poetry schema progress)
  batbox_indigenous(phase4-bats-gadget-${section}-indigenous
    tests/phase4/bats_gadget_${section}_indigenous.cpp batbox_bats)
endforeach()
batbox_indigenous(phase4-bats-resume-typed tests/phase4/bats_resume_typed_indigenous.cpp batbox_bats)
