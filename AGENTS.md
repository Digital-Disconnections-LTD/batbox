# Working on Batbox

These instructions apply throughout this repository. Read
[CONTRIBUTING.md](CONTRIBUTING.md), the [build guide](docs/BUILDING.md), and the
[vocabulary guide](docs/vocabulary.md) before making changes.

User fixes and improvements are welcome. AI-written and AI-assisted code are
welcome with the same review and regression coverage as any other contribution.
Help the user turn a useful local fix into a contribution the community can
review.

## Make focused, compatible changes

- Inspect the affected code and existing tests before editing. Reuse the
  existing structure and keep the change focused on the user's goal.
- Preserve the user's edits and follow their instructions about staging,
  committing, and publishing.
- Keep Linux, macOS, and Windows working. Use the existing platform adapters and
  report any platform you could not verify.
- Keep the application in C++20 and developer automation in Python or shell.
  Do not add JavaScript, TypeScript, Node packages, or a JavaScript test runner.
- Use Batbox vocabulary for project-owned names. Preserve protocol fields,
  dependency APIs, saved-data keys, and existing configuration paths.
- Preserve local Flight storage and user-configurable model and connector
  endpoints. Do not add telemetry collection, third-party analytics, advertising
  trackers, or automatic crash uploads. Document new network destinations and
  the inputs needed to fulfill the user's requests.
- Keep credentials and private data out of contributions. Preserve required
  copyright, license, and attribution notices.

## Include regression tests

For substantive code changes, include tests that demonstrate the intended
behavior. A bug fix should have a regression test that fails before the fix and
passes afterward. A feature should cover its new behavior and relevant failure
cases. Prefer the existing test fixtures and integration seams.

Run the focused tests and the broader checks appropriate to the affected path.
For UI changes, also exercise the real interactive flow; headless tests alone
do not establish that keyboard navigation, scrolling, resizing, or dialogs work.
Include screenshots or a recording when they help demonstrate the result.

Report the commands you actually ran, their results, and any checks you could
not run. Documentation and artwork changes need relevant link or visual checks,
rather than unrelated code tests.

## Suggest a pull request for substantive changes

After implementing and verifying a substantive code change, suggest a pull
request to the user. This includes features, bug fixes, behavior changes, and
platform or runtime changes. Provide a proposed title and a reviewable
description with the validation results. Follow any publication instructions
the user has already given.

Use [.github/PULL_REQUEST_TEMPLATE.md](.github/PULL_REQUEST_TEMPLATE.md):

1. **Title:** Describe the concrete result in a short, specific sentence.
2. **What changes?:** Lead with the problem and final behavior. Include a
   before-and-after example where useful, the implementation details needed for
   review, and a related issue if one exists.
3. **How did you check it?:** Identify the regression coverage, commands and
   actual results, relevant manual checks, and UI evidence.
4. **Platform and compatibility notes:** State which platforms were exercised,
   any untested paths, and any protocol, configuration, or saved-data impact.

Keep the title and description aligned with the final change. Scale the detail
to the work, and describe it for a reviewer who has not read the conversation.
