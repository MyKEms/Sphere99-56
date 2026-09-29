# Object lifetime model

This note records the bounded lifetime boundaries implemented in the engine.
It is a design boundary, not a claim that every object uses one uniform
ownership model.

## Why the no-op `CRefPtr` is not replaced globally

The current code uses several incompatible lifetime conventions:

- `CRefPtr<T>` is a borrowed raw pointer in this reconstruction. Its retain
  and release operations are no-ops.
- Some `CGObList` and array types own their elements, while reference arrays,
  UID lookups, caches, and client/account relationships only borrow them.
- World objects already use an end-of-tick deletion queue. Clients, accounts,
  parties, and chat channels now have owner-specific deferred queues, while
  resource definitions remain borrowed through their lookup tables.
- A global intrusive implementation would therefore need an ownership audit,
  adopted-versus-borrowed pointer semantics, cycle handling, and a common
  virtual destruction contract before it could be safe.

Changing only `CRefPtr` would risk both leaks and double destruction. That work
is intentionally deferred until those contracts are explicit.

## Deferred owner boundaries

The following owners keep their existing ownership model and defer destruction
until the callbacks and traversals that can borrow them have finished:

* `CClient` queues an idempotent delete after receive, dispatch, and socket
  flush, and drains the queue again during shutdown.
* Temporary `CAccount` objects retain their character UID list while character
  teardown detaches each character. Account callbacks run before the account
  queue is drained.
* `CParty` queues disbanded parties until client dispatch/flush completes, then
  clears member back-pointers before exactly-once destruction.
* `CChatChannel` queues closed channels until dispatch/socket flush completes,
  clears client back-pointers, and drains the queue during shutdown.

Each queue is owner-specific because the surrounding traversal and detach
invariants differ. None of them changes `CRefPtr` into an owning reference.

### Client boundary

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
reference-counting implementation.

## Test contract

The synthetic lifetime fixtures include bounded headless login, callback,
disconnect, party, and chat scenarios. The account, party, and channel probes
assert callback-before-destruction ordering, exactly one destruction marker,
and a clean bounded shutdown. Native and ASan/UBSan runs use generated fixture
data; leak detection remains a separate concern because the sanitizer job
intentionally disables it, so each owner queue is also drained explicitly at
shutdown.

The soak uses only generated fixture data. It must never be pointed at a
production `save/` or `accounts/` directory.

## Resource definitions

Resource definitions use a different boundary from world objects and the
runtime owners above. `CResourceDef` and `CResourceLink` instances are borrowed
through the resource manager's lookup arrays; those arrays do not own or delete
the definitions during a server tick. A normal resource resync only closes the
script files and keeps the definitions linked while the replacement scripts are
loaded. A full shutdown closes the world first, then unlinks and clears the
resource lookup arrays, so world objects no longer execute callbacks against a
resource definition when that boundary is crossed. Account cleanup follows the
resource boundary and does not retain a resource pointer after the world has
closed.

This means the current engine has no same-tick resource-definition deletion path
that needs another deferred queue. If resource hot-unload is changed to destroy
definitions rather than unlink them, it must become an explicit owner boundary
with an end-of-tick queue and an ASan callback fixture before that change is
enabled.
