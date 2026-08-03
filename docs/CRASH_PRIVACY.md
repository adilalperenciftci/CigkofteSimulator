# Crash reporting and privacy

- A crash bundle is attached only to a report the user explicitly chose to send.
  There is no automatic upload.
- The log, `CrashContext.runtime-xml`, the minidump and the system information
  can all contain personal data.
- `Collect-CrashBundle.ps1` redacts text that looks like an API key, token,
  password, DSN or credential. Human review before sharing is still required.
- Usernames, full file paths, IP addresses, email addresses, machine names and
  free-text comments need reviewing separately.
- Shipping logs must not write auth headers, secrets in URL queries, personal
  messages or full user directories.
- Sentry or any other endpoint is enabled only after explicit consent, a privacy
  policy, a stated retention period and a configured DSN.
- The Sentry plugin exists under `Plugins/Sentry` on at least one development
  machine and is gitignored. It is not adopted: the `.uproject` carries
  `"Sentry": { "Enabled": false }`, and release validation fails if any Sentry or
  crashpad file reaches a package. Adopting it means changing this document, the
  consent flow and the project descriptor together — not enabling a plugin.
- Symbol archives are not published with a release; access stays restricted.
- A packaged Unreal build does not send crashes to Epic by default, and no
  telemetry starts until a custom endpoint is configured.
