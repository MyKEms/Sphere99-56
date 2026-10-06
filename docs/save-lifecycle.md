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
interrupted.  The commit is recorded in the manifest as soon as the second
rename and its directory sync have completed; re-reading the published files
afterwards is only a diagnostic and never discards the committed generation.

If the commit record is missing, the restart loads the live pair as the
published generation only when both files carry the pending generation's
`SAVECOUNT` and each was replaced after the generation backed it up: its
recorded backup exists, and the live file is a different file (device and
inode) with different contents.  The restart then drops the stale pending
record, so the next save backs the pair up before replacing it.  The count
alone is not proof.  The counter advances only after a committed save, so the
first save after a start-up writes the count that the loaded pair, and the
backups taken of it, already carry; a publication interrupted between the two
renames leaves a new world file next to the old character file, both with
that count.  Whenever publication is not proven this way (no backup was
recorded, a live file is still its hard-linked backup, or a copied backup has
the same contents), the restart loads the recorded backups as for any other
interrupted generation and keeps the pending record, so the retry reuses
them.

The account file follows the same keep-then-replace rule: the previous
`sphereaccu.scp` remains readable while a new generation is serialized to a
temporary file, then the temporary file is renamed into place.  The directory
is synchronized after that publication so the rename survives a sudden restart.

A retry of a failed generation reuses every backup that the pending manifest
records as already taken (world, characters, accounts, and the server list)
instead of overwriting it with a file published by the failed attempt.  The
manifest records each backup's actual path (`ARCHIVE_W`, `ARCHIVE_C`,
`ARCHIVE_A`, `ARCHIVE_S`) once the backup's directory has been synchronised,
and the retry and the start-up use that path.  A recomputed name could differ:
the server list backup carries the current date, and `BACKUPLEVELS`
selects the other names.  A manifest without recorded paths falls back to the
computed names.  A component that the failed attempt did not reach is backed
up by the retry.  If a recorded backup has disappeared, the retry stops
instead of rotating again; the log names the missing file and the manifest
(`sphere.save.pending`) to remove if that backup cannot be restored.

Startup applies the same fail-closed rule to a pending manifest: a component
whose recorded archive is missing is not silently taken from the live pair.
It reports the missing path and leaves recovery for an operator to repair or
abandon explicitly.  When `SAVEBACKUPFALLBACK=1` is enabled for an ordinary
(non-pending) load, a pair with only one readable `SAVECOUNT` can still select
the corresponding backup level; the fallback walk reports each selected file
before loading it.  If no usable backup exists, the fatal diagnostic names
`SAVEBACKUPFALLBACK=1` as the opt-in.

A backup is taken as a hard link to the live file, so the live name never
disappears.  Where a hard link is not possible, the live file is copied to a
temporary name that is then renamed over the backup name; the backup name is
never opened for writing.  An old backup that cannot be removed stops the
save, because it may still be a hard link to the live file.

Save backups retain Sphere's stock names and rotation rule.  For generation
`N`, the backup level is selected by the number of trailing zero octal digits
in `N`; the resulting names are `sphereb<level><slot><kind>.scp`, where the
kind is `w` (world), `c` (characters), or `a` (accounts).  This keeps existing
0.99 save trees readable while making the publication boundary explicit.

Current files are authoritative on startup.  The live world and character
files must carry the same `SAVECOUNT`; a pair whose counts differ, or where
only one file carries a count, is rejected.  The count is read only from a
file's header (the keys before the first section, or a leading `[SPHERE]`
section as older servers write it) and the key matches in any case, so
`SaveCount=` is recognised and a global variable named `SAVECOUNT` in
`[VARNAMES]` is never taken for the count.  Legacy files without any
`SAVECOUNT` header (for example `[EOF]`-only placeholders) are accepted only
when neither file of the pair carries one.  Older backups are considered only
for an interrupted transaction recorded by the pending manifest, or when the
operator explicitly enables `SAVEBACKUPFALLBACK=1`.  Each selected backup is
reported with its path, its own `SAVECOUNT` and save time (the file's
modification time), and the wall-clock time of the selection.

The daily log retains the stock event line `World data saved (...)`, naming
the published live world file, and also records the engine's save diagnostics
(`World save started` / `World save ended`).  The event is emitted only after
the save has reached its publication boundary.
