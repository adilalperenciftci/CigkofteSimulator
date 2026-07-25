# QA log

Only executed commands and observed results. Nothing here is predicted.

## 2026-07-26 — baseline

| Check | Command | Result |
|---|---|---|
| Static | `python Tools/check_sources.py` | clean: 13 CSVs, 695 keys, 2 languages |
| Build | `Build.bat CigkofteSimulatorEditor Win64 Development` | Succeeded |
| Tests | `Automation RunTests Cigkofte` | 50 passed, 0 failed, exit 0 |
| Data load | runtime log | 13/13 CSVs loaded, 0 warnings |

## 2026-07-26 — slice 0.2 + 0.3

| Check | Command | Result |
|---|---|---|
| Static | `python Tools/check_sources.py` | clean (Inspection.csv now 15 rows) |
| Build | `Build.bat CigkofteSimulatorEditor Win64 Development` | Succeeded |
| Tests | `Automation RunTests Cigkofte` | **52 passed, 0 failed**, exit 0 |

New tests observed passing:

- `Cigkofte.Staff.PackageKeepsDoughSpice`
- `Cigkofte.Inspection.ComplaintRaisesOddsWithoutGuaranteeing`

## Not yet executed

Scenario tests required by the commercial-demo standard that do not exist yet:
sales parity, bulk-order lifecycle, day-long event teardown, power-cut
refrigeration, closure blocking customers, save migration, placement round trip,
pooled seat release, localization placeholder parity.
