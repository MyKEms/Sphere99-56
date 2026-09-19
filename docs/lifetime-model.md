# Client lifetime model

This note records the first bounded step for issue #13. It is deliberately a
design boundary, not a claim that the whole engine has a uniform ownership
model.

## Why the no-op `CRefPtr` is not replaced globally

The current code uses several incompatible lifetime conventions:

- `CRefPtr<T>` is a borrowed raw pointer in this reconstruction. Its retain
  and release operations are no-ops.
- Some `CGObList` and array types own their elements, while reference arrays,
  UID lookups, caches, and client/account relationships only borrow them.
- World objects already use an end-of-tick deletion queue, but clients,
  accounts, parties, chat channels, and resources still have independent
  cleanup paths, including explicit `delete this` paths.
- A global intrusive implementation would therefore need an ownership audit,
  adopted-versus-borrowed pointer semantics, cycle handling, and a common
  virtual destruction contract before it could be safe.

Changing only `CRefPtr` would risk both leaks and double destruction. That work
is intentionally deferred until those contracts are explicit.

## First implementation boundary: clients

`CClient` is traversed as a raw intrusive-list record during receive, message
dispatch, and socket flush. A disconnect can currently destroy the record from
inside one of those traversals. The first boundary now:

1. makes `CClient::DeleteThis()` idempotent;
2. performs the existing detach, account, chat, and output cleanup once;
3. removes the client from the active list and puts it in a server-owned
   pending-deletion list;
4. drains that list only after receive, dispatch, and flush finish; and
5. drains it again during server shutdown.

The world-object deletion queue is not reused: it has different invariants and
is owned by `CWorld`.

The queue keeps a client alive through the current tick, but it is not a
reference-counting implementation. Other object families remain out of scope
for this slice.

## Test contract

The synthetic fixture includes a bounded headless login/walk/disconnect soak.
It alternates graceful and abrupt socket closes, validates the structurally
framed game-start response, sends movement packets, and checks that the login
socket remains available after every cycle. CI runs the soak with the existing
ASan/UBSan build. Leak detection remains a separate concern because the current
CI sanitizer job intentionally disables leak detection; the server-side queue
is therefore also drained explicitly at shutdown.

The soak uses only generated fixture data. It must never be pointed at a
production `save/` or `accounts/` directory.
