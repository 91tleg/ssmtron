# Repository Guidelines

## Verification

Run tests or a firmware build when a change is significant or a specific concern needs checking. Do not run them after every small edit. When a device build is needed in this workspace, use the Nordic toolchain environment:

```sh
cd /opt/nordic/ncs/v3.2.4
nrfutil sdk-manager toolchain launch --ncs-version v3.2.4 -- \
  west build -b nrf52840dk/nrf52840 \
  /Users/maxchou/Desktop/ssmtron/firmware/app \
  -d /Users/maxchou/Desktop/ssmtron/firmware/build --pristine
```

## Code Changes

Follow applicable MISRA C guidance. Wrap integer macro values with spaced parentheses, for example `#define RETRY_MS ( 500u )`. Add comments where they explain a decision, invariant, or non-obvious behavior. Use plain punctuation and avoid em dashes in documentation and messages.

## Commits and Pull Requests

Create a new branch from `main` for each feature. Use
`<type>/<short-kebab-case-description>` for branch names, such as
`feat/ble-status-display`, `fix/uart-overflow`, or `chore/update-toolchain`.

Use Conventional Commits. Keep the subject within 50 characters and body lines within 72 characters. Use bullets in the body when details are needed. Describe the changes made and their effect. Do not copy the request into the commit message, add yourself as a co-author, or refer to uncommitted files or internal instructions.
