# cLog++ design notes

How the pieces fit, what the hot path costs, and the trade-offs behind the
defaults. Read this before changing `logger.hpp` or `core.hpp`.

## The life of a log call

```
log.info("event").kv("k", 1);
```

1. `Logger::info` checks the level (one relaxed atomic load). For a filtered
   level it returns an inactive `LogRecord`; every `kv()` on it is a branch and
   nothing else. That is the 4 ns "filtered" line in the benchmark.
2. An active record captures the time, the source location (already in hand,
   it is a defaulted argument), the thread id if enabled, a `shared_ptr` to the
   current bound fields, and a copy of the thread's scoped-context fields.
3. `kv()` appends `,"k":1` to the record's `FieldSet`: one contiguous string
   buffer plus a small vector of spans. Values are encoded immediately, so a
   record is ready to format the moment it is complete and the formatter never
   touches user types.
4. When the record goes out of scope it is submitted. Sync mode formats and
   writes on the calling thread. Async mode moves the entry into the queue and
   returns.
5. The worker pops up to `batch_size` entries under one lock acquisition,
   releases the lock, and then formats and writes each entry. Sinks without a
   formatter share one formatted line; sinks with their own get their own.

## Async queue

The queue is a single-producer/single-consumer ring buffer wrapped in a mutex,
which makes it multi-producer safe. A lock-free MPSC queue was considered and
rejected: with the batch drain, the lock is held for a few dozen nanoseconds
per entry, and the simplicity pays for itself in the invariants that follow.

- `pending_` counts entries enqueued but not yet written. `flush()` waits for
  it to reach zero, so "flush returns" means "every sink has seen every entry
  submitted before the call".
- `stop_` is set by the destructor. The worker keeps draining until the queue
  is empty and only then exits, so shutdown never loses entries that were
  accepted.
- With `OverflowPolicy::Block`, a producer that finds the queue full waits on
  `not_full_`. With `DropNewest` it increments `dropped_` and returns.
  Entries submitted after the destructor has started are dropped either way.

Async is not a throughput feature on a single thread: the same work happens on
another core, and the hand-off costs a mutex round trip. It is a latency
feature. The calling thread pays for capturing the record (about 0.1 us) and
nothing for formatting or I/O.

## Timestamps without gmtime

`detail::format_iso8601` converts the epoch count to a civil date with integer
arithmetic (Howard Hinnant's `civil_from_days`) and writes digits by hand.
There is no `gmtime`, no `snprintf`, no locale, no lock and no thread-local
cache. This was the single biggest performance fix in 0.3: the first draft
cached the `gmtime` result in a `thread_local`, and on MinGW-w64 (emulated
TLS) every access to it cost around 0.7 us, five times the rest of the call.

## Thread-local state

Only two things are per-thread, and both are deliberately trivial:

- The scoped-context pointer. The `FieldSet` storage lives inside the
  outermost `ScopedContext` object on that thread; the thread-local part is
  a single pointer to it. On MinGW-w64 the pointer lives in a native Win32
  TLS slot (`TlsAlloc`) instead of `thread_local`, for the reason above.
- The cached OS thread id on Linux and macOS. On Windows
  `GetCurrentThreadId` reads the TEB and needs no cache.

Avoiding non-trivially-destructible thread-locals is also a correctness
matter: some toolchains free a thread's TLS block before running C++
`thread_local` destructors, which turned a `thread_local FieldSet` into a
use-after-free at thread exit during development.

## Sinks and locking

`sink_mutex_` serialises everything that touches the sink list: adding and
clearing sinks, changing the default formatter, formatting, writing and
flushing. Two scratch strings (`default_line_`, `sink_line_`) live under the
same lock and are reused, so a steady-state log call allocates only for the
record itself.

Sink exceptions are caught per call. A misbehaving sink cannot take the worker
down (which would deadlock every `flush()`) or starve the other sinks; the
failure is counted and reported through the error handler.

Per-sink levels are checked on the worker, after the logger-level check on the
producer. The logger level is the cheap, global gate; sink levels only route.

## Bound fields are copy-on-write

`bound_` is a `shared_ptr<const FieldSet>`. Binding builds a new set and swaps
the pointer under a small mutex; records take a reference (one atomic
increment) at creation. An entry sitting in the async queue is therefore
formatted with the bindings that were active when it was logged, and binding
is safe from any thread at any time.

## Encoding rules

- Strings are escaped per RFC 8259: `"`, `\`, control characters as `\uXXXX`
  or the short forms. Non-ASCII bytes pass through untouched, so valid UTF-8
  in stays valid UTF-8 out.
- Doubles use `%.17g`, which round-trips. NaN and infinities have no JSON
  representation and become `null`.
- Nothing is de-duplicated or reordered. The output is exactly what was
  logged, in order: fixed prefix, bound, context, record fields, then the
  optional `tid`, `file`, `line`, `func`.

## Platform notes

- Windows API functions used by the library (`GetCurrentThreadId`, `TlsAlloc`,
  `TlsGetValue`, `TlsSetValue`) are declared directly rather than through
  `<windows.h>`. The declarations match the SDK's, so including `<windows.h>`
  before or after cLog++ is fine (there is a test for it).
- `DailyFileSink` is the only header that uses `<filesystem>`. GCC 8 needs
  `-lstdc++fs`; the CMake target adds it.
- MSVC needs `/permissive-` only for the tests; the library itself compiles
  under the default mode too.
