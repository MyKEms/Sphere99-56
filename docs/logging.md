# Server logging

The server writes a daily text log when `LOG=logs/` (or another directory) is
configured. Files are named `sphereYYYY-MM-DD.log`. The formatter follows the
0.99 daily-log shape: `HH:MM:` is emitted for runtime events, level words are
`CRITICAL:`, `ERROR:`, and `WARNING:`, and script failures include a
`(file,line)` context before the message. Startup/load events retain the stock
plain-line form while the server is loading.

The engine's internal levels map to the stock vocabulary as follows:

| Engine level | Daily log level |
| --- | --- |
| `LOGL_TRACE` | `DEBUG` |
| `LOGL_EVENT` | `INFO` |
| `LOGL_WARN` | `WARNING` |
| `LOGL_ERROR` | `ERROR` |
| `LOGL_CRIT` / `LOGL_FATAL` | `CRITICAL` |

`LOGMASK` continues to select event groups for the daily file. The Linux
stderr channel remains available for fatal, critical, and error diagnostics;
the `SPHERE_LOG_*` macros use `DEBUGLEVEL` for their separate tooling trace.

`tools/logdiff.py` replaces names, addresses, paths, serials, and other
run-specific values with placeholders before comparing two daily logs. It
reports message classes present on only one side and count ratios for classes
present in both:

```sh
python3 tools/logdiff.py linux.log windows.log
python3 tools/logdiff.py --json linux.log windows.log
```
