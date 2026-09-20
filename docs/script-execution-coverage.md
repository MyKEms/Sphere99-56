# Script execution coverage

Execution coverage is disabled unless `SCRIPTEXECUTIONREPORT` is set in
`sphere.ini`. The configured path is relative to the server's working directory
unless it is absolute. The server writes a report on clean shutdown. An admin
can also write a snapshot while the server is running with
`SERV.SCRIPTCOVERAGEREPORT`.

The report includes every registered section, including sections with zero
hits. It tracks functions, supported resource trigger blocks, dialog layouts
and button handlers, and menu and skill-menu options. Each entry records the
resource type and name, section kind and name, hit count, and source file.

Top-level report fields:

- `loaded`: tracked sections plus registrations omitted at the memory cap.
- `distinct`: section entries stored in the report.
- `executed`: stored section entries with at least one hit.
- `total_hits`: sum of recorded section hits and unassigned overflow hits.
- `overflow_sections`: section registrations beyond the 65,536-entry cap.
- `overflow_hits`: hits that could not be assigned to a stored section.

When `overflow_sections` or `overflow_hits` is nonzero, the report is
incomplete and `executed / loaded` is only a lower-bound coverage measure. With
no overflow, `loaded` is the exact denominator and `executed / loaded` is the
executed section percentage.
