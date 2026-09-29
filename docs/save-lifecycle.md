# Save lifecycle

World and character saves are written to `sphereworld.scp.tmp` and
`spherechars.scp.tmp`.  Each file is flushed and synchronised, then checked
for a matching `SAVECOUNT` and a terminal `[EOF]` section before publication.
The temporary files replace the live names with atomic renames; the previous
live files are preserved as the paired backup only after both temporary files
pass validation.  A pending save manifest records which backup components are
available, so a restart can select the last complete pair if publication was
interrupted.

Backup names retain the stock rotation convention.  For save count `N`, the
first non-zero octal digit determines the level (`b0N`, `b1N`, `b2N`, ...), and
the low three bits select the slot.  World and character files use the `w` and
`c` suffixes respectively, for example `sphereb01w.scp` and
`sphereb01c.scp`.

Current files are authoritative on startup.  Older backups are considered
only for an interrupted transaction recorded by the pending manifest, or when
the operator explicitly enables `SAVEBACKUPFALLBACK=1`; an enabled fallback is
reported with the selected path, save count, and wall-clock time.
