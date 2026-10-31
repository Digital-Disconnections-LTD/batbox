foreach(section IN ITEMS instructions_path instructions_poetry instructions_frontmatter instructions_scan
    skills_loader skills_roster skill_gadget_validation skill_gadget_poetry skill_gadget_schema skill_gadget_inline skill_gadget_factory skill_process
    skill_usage skill_remote skill_remote_gate)
  add_executable(batbox-${section}-probe tests/phase4/${section}_probe.cpp)
  target_link_libraries(batbox-${section}-probe PRIVATE batbox_skills)

endforeach()
batbox_indigenous(phase4-skill-gadget-factory-indigenous tests/phase4/skill_gadget_factory_indigenous.cpp batbox_skills)
batbox_indigenous(phase4-skill-remote-factory-indigenous tests/phase4/skill_remote_factory_indigenous.cpp batbox_skills)
