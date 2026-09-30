# Save lifecycle

The server publishes a completed save only after its files have been written,
flushed, and closed successfully.

World and character saves are written to `sphereworld.scp.tmp` and
`spherechars.scp.tmp`.  Each file is flushed and synchronised, then checked
for a matching `SAVECOUNT` and a terminal `[EOF]` section before publication.
The temporary files replace the live names with atomic renames, and the
directory is synchronised after each rename; the previous live files are
preserved as the paired backup only after both temporary files pass
validation.  A pending save manifest records which backup components are
available, so a restart can select the last complete pair if publication was
interrupted.

The account file follows the same keep-then-replace rule: the previous
`sphereaccu.scp` remains readable while a new generation is serialized to a
temporary file, then the temporary file is renamed into place.  The directory
is synchronized after that publication so the rename survives a sudden restart.

A retry of a failed generation reuses every backup that the pending manifest
records as already taken instead of overwriting it with a file published by
the failed attempt.  If a recorded backup has disappeared, the retry stops
instead of rotating again.

Save backups retain Sphere's stock names and rotation rule.  For generation
`N`, the backup level is selected by the number of trailing zero octal digits
in `N`; the resulting names are `sphereb<level><slot><kind>.scp`, where the
kind is `w` (world), `c` (characters), or `a` (accounts).  This keeps existing
0.99 save trees readable while making the publication boundary explicit.

Current files are authoritative on startup.  The live world and character
files must carry the same `SAVECOUNT`; a pair whose counts differ, or where
only one file carries a count, is rejected.  Legacy files without any
`SAVECOUNT` header (for example `[EOF]`-only placeholders) are accepted only
when neither file of the pair carries one.  Older backups are considered only
for an interrupted transaction recorded by the pending manifest, or when the
operator explicitly enables `SAVEBACKUPFALLBACK=1`; an enabled fallback is
reported with the selected path, save count, and wall-clock time.

The daily log retains the stock event line `World data saved (...)`, naming
the published live world file, and also records the engine's save diagnostics
(`World save started` / `World save ended`).  The event is emitted only after
the save has reached its publication boundary.
