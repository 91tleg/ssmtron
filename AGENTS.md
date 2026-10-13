# Repository Guidelines

## Reasoning Before Changes

- Trace the affected path from input or data source through state changes to
  the visible result. Read the callers, callees, and relevant HAL behavior
  before choosing a fix.
- Identify the assumptions the change introduces or invalidates: units,
  decimal precision, signed ranges, timing, ownership, initialization order,
  and error handling. Check existing helpers and comments against those
  assumptions. A new caller can expose a bug in an unchanged helper.
- For state machines, walk concrete event sequences with timestamps. Include
  normal operation, bounce, repeated edges, delayed polls, release near a
  hold threshold, and a new event arriving between reads or polls. Distinguish
  an event's timestamp from when the loop processes it.
- Establish invariants before implementing. Examples: an ended press cannot
  gain hold time; a dial reads only its selected parameter; a settled needle
  displays its target; page content is set before the first draw.

## Verification

Run tests or a firmware build when a change is significant or a specific concern needs checking. Do not run them after every small edit. When a device build is needed in this workspace, use the Nordic toolchain environment:

```sh
cd /opt/nordic/ncs/v3.2.4
nrfutil sdk-manager toolchain launch --ncs-version v3.2.4 -- \
  west build -b nrf52840dk/nrf52840 \
  /Users/maxchou/Desktop/ssmtron/firmware/app \
  -d /Users/maxchou/Desktop/ssmtron/firmware/build --pristine
```

### Tests That Check Behavior

- For a bug fix, add a focused regression test when practical and confirm it
  fails with the faulty behavior restored. Restore the fix afterward. Test
  the requirement independently of the implementation's formula or helper.
- Test through the real affected path. Capture the first frame during the
  initialization call; animate through the motion module to check where a
  needle settles; inspect intermediate frames when claiming every redraw is
  correct. Checking state afterward or assigning angles directly does not
  verify those behaviors.
- Keep fakes faithful to production contracts, including edge collapsing,
  event ordering, cached data, and failure behavior. Do not replace a latch
  with a FIFO or assume only the selected reading is fresh unless production
  actually guarantees it. State what a fake cannot model.
- Choose boundary cases from the assumptions: negative angles and crossing
  zero, integer limits, non-round sensor readings, both panel sizes, and
  timestamps just before and after thresholds. Check error paths when the
  changed code handles or depends on success.
- Use the relevant host tests, preview renders, clang-format, and the pinned
  CI cppcheck/MISRA command for significant firmware changes. Inspect changed
  previews. Check RAM and stack implications when adding buffers or local
  arrays; host tests alone do not establish device memory headroom.

### Review Before Reporting Completion

- Review the final diff as a reviewer would. Trace the fix again, look for
  the same pattern in neighboring paths, and check whether the tests hide an
  invalid assumption. Passing checks do not replace this review.
- Honor the user's verification scope, including requests not to build the
  device. Report exactly what ran and passed, what was inspected, and what
  remains unverified. Distinguish host compilation, a device build, visual
  inspection, and hardware testing.
- Do not claim exhaustive coverage or hardware reliability from a few edge
  cases. Describe concrete remaining limitations and any failed checks.

## Code Changes

Follow applicable MISRA C guidance. Wrap integer macro values with spaced parentheses, for example `#define RETRY_MS ( 500u )`. Add comments where they explain a decision, invariant, or non-obvious behavior. Use plain punctuation and avoid em dashes everywhere in the codebase. Use american english only. include should use full path.

## Commits and Pull Requests

Create a new branch from `main` for each feature. Use
`<type>/<short-kebab-case-description>` for branch names, such as
`feat/ble-status-display`, `fix/uart-overflow`, or `chore/update-toolchain`.

Run clang-format before any commits. Use Conventional Commits. Keep the subject within 50 characters and body lines within 72 characters. Use bullets in the body when details are needed. Describe the changes made and their effect. Do not copy the request into the commit message, add yourself as a co-author, or refer to uncommitted files or internal instructions.
