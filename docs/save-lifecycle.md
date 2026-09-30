# Save lifecycle

The server publishes a completed save only after its files have been written,
flushed, and closed successfully.  The account file follows the same
keep-then-replace rule: the previous `sphereaccu.scp` remains readable while a
new generation is serialized to a temporary file, then the temporary file is
renamed into place.  The directory is synchronized after that publication so
the rename survives a sudden restart.

Save backups retain Sphere's stock names and rotation rule.  For generation
`N`, the backup level is selected by the number of trailing zero octal digits
in `N`; the resulting names are `sphereb<level><slot><kind>.scp`, where the
kind is `w` (world), `c` (characters), or `a` (accounts).  This keeps existing
0.99 save trees readable while making the publication boundary explicit.

The daily log retains the stock event line `World data saved (...)` and also
records the engine's save diagnostics.  The event is emitted only after the
save has reached its publication boundary.
