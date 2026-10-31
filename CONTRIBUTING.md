# Welcome to the Roost 🦇

Bring your curiosity. A good contribution can be five lines of C++, one clearer
sentence, a reproducible bug, or a terminal screenshot that helps us see a problem.
Your fixes and improvements are welcome, however small. You do not need to wait
for a listed Mission to contribute something useful.

Batbox is a Digital Disconnections community project. Privacy and control over
your own devices guide what we build. Batbox does not collect user data or send
telemetry to the project or third-party analytics services. Do not add usage
tracking, advertising trackers, or automatic crash uploads. There is no opt-out
setting because there is no collection to disable.

Keep model and connector endpoints configurable, preserve local Flight storage,
and document any new network destinations and the inputs needed to fulfill the
user's requests. Model providers, MCP servers, and other connected services
control their own data practices; our privacy promise covers Batbox itself.
Keep credentials and private project content out of issues, fixtures,
screenshots, and pull requests.

## AI code is welcome

AI-written and AI-assisted code are welcome. Bring regression tests with it:
for a bug fix, show a test that fails before the fix and passes after it; for a
feature, cover the new behavior. Understand the code you submit, review it, and
run the relevant checks yourself. The same quality bar applies to all code,
however it was written.

Documentation and artwork contributions should include a preview or a clear
description of what you checked. A prose edit does not need a code test.

## Features we would love help with

- **A better view of the crew.** Improve handling of multiple Bats: make their
  status and progress easier to follow, let users enter their conversations and
  send follow-ups, and make returning to the leader feel effortless. The existing
  `/bats` and `/leader` commands are starting points to improve.
- **A Flight manager for one busy window.** Keep multiple independent sessions
  running on different projects or Missions in one Batbox window. Make it easy to
  switch between them while the others keep working, with each conversation's
  context kept separate.
- **Find the little UI gremlins.** Much of the testing so far has focused on
  headless Bats. There are likely many small bugs, especially in the interactive
  UI. Real terminal testing, clear reproductions, and small fixes are especially
  welcome: try resizing, scrolling, keyboard navigation, conversation switching,
  and permission dialogs on Linux, macOS, and Windows.

These are invitations to collaborate. If you spot another useful improvement,
bring it to the Roost.

## Choose your first Mission

| You enjoy… | A good place to begin |
| --- | --- |
| Writing | Walk through the getting started guide and improve a confusing step. |
| C++ | Fix a focused bug in streaming, Gadgets, or the terminal interface. |
| Testing | Try a real Flight on Linux, macOS, or Windows and share exact results. |
| Design | Improve legibility, keyboard navigation, color contrast, or Halloween art. |
| Research | Test an Anthropic Messages backend or an MCP connector. |

Before a large change, open an issue with the problem and your proposed result.
For a small fix, a pull request with a clear explanation is welcome directly.
AI coding assistants should also follow the repository's [AGENTS.md](AGENTS.md).

## Make it easy to review

1. Work on a branch and keep the change focused.
2. Follow the existing C++20 structure and the [Batbox vocabulary](docs/vocabulary.md).
3. Add regression coverage for behavior changes and run the relevant checks from
   the [build guide](docs/BUILDING.md).
4. Explain the visible behavior, why it changed, and what you tested.
5. Include screenshots for interface changes and your OS/compiler for platform fixes.

Use the [pull request template](.github/PULL_REQUEST_TEMPLATE.md). Give the PR a
title that describes the result, explain the problem and final behavior, list
regression tests and their actual results, and note platform or compatibility
limits. A short before-and-after example often makes a fix easier to review.

Keep Linux, macOS, and Windows paths working. Put platform differences behind the
existing platform adapters. A fix for one system should not erase another system’s
implementation. If you cannot test a platform, say so in the pull request.

The application is C++20. Developer automation uses Python and shell. Do not add
JavaScript, TypeScript, Node packages, generated script bundles, or a JS test runner.
Use the existing libraries and test seams before adding a dependency.

Our names are playful; our contracts stay precise. Do not rename protocol fields,
saved-data keys, standard-library APIs, or dependency symbols as a branding change.
Include migration behavior when a data format really needs to change.

## Report something spooky

Give us the command or steps, expected behavior, actual behavior, platform, and
relevant logs. A small reproduction is a gift. Remove API keys, private URLs,
project contents, and personal data from anything you share.

For a security issue, follow [SECURITY.md](SECURITY.md). For everyday collaboration,
follow the [Code of Conduct](CODE_OF_CONDUCT.md).

Contributions to Batbox are provided under the project’s [MIT license](LICENSE).
Keep existing copyright, license, and attribution notices with any reused material.
Thank you for helping this little bat fly.
