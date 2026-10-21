# Native tile scheduler contract

`TileScheduler` adds background region reads above the synchronous image-server
API. C++ owns the queue, threads, generations, cache and publication decisions.
Python exposes the same scheduler through nanobind. Workers never call Python.
The current ImGui viewer has not yet been connected to this scheduler.

## Identity and publication

A scheduler retains one immutable image server for its entire lifetime. Create
a new scheduler when changing the image source. Tickets are monotonically
increasing and local to that scheduler; results include their request generation.
The initial generation is 1. `advance_generation()` marks all unconsumed tickets
stale and advances that number. Queued stale work is not decoded; an active stale
read may finish but cannot publish pixels or enter the cache.

An identical region in the current generation shares one ticket until consumed.
Repeated submission raises its queued priority if necessary. This is a shared
request, not separate subscriptions: canceling the ticket cancels it for all
submitters, and its terminal result can be consumed only once. Higher integer
priority starts first among eligible queued requests; equal priorities use FIFO
ticket order. Already admitted reads cannot be preempted.

`take(ticket)` returns queued/running status without removing the ticket. For
ready, failed, canceled or stale status it returns and consumes the terminal
result. Unknown and previously consumed tickets raise an error. A caller that
already took a result must still check its generation before applying it to a
changed view; generation changes cannot revoke objects held outside the scheduler.

## Memory and backpressure

`SchedulerOptions` is copied on construction. Defaults are two workers, a 64 MiB
cache payload budget, a 64 MiB work budget, 256 outstanding tickets and 1024
cache entries. Zero cache bytes or zero entries disables caching; worker count,
work budget and ticket count must be positive. Worker count is limited to 64.

The work reservation is twice the decoded RGBA byte count, covering output plus
the current OpenSlide conversion buffer. Admission reserves those bytes before
decoding. A ready result retains its reservation until consumed, canceled or
made stale. Cancellation of an active read retains the reservation until the
reader actually returns. Oversized individual tiles are rejected. Requests wait
in the queue when the work budget is exhausted; submission fails explicitly when
the ticket-count limit is reached. Queue payloads contain metadata, not pixels.

The cache separately limits decoded payload bytes and entry count. Eviction uses
least recently used order. Cached immutable pixels remain reusable across view
generations of the same source. Cache and result ownership can share an allocation;
accounting is conservative. Consumers that retain returned buffers own memory
outside these scheduler budgets. Reader-internal caches, document images, metadata
and allocator overhead are also outside the payload limits.

Ready results apply backpressure. `wait_idle(timeout_seconds)` may time out when
queued work cannot start until the caller consumes ready tickets. GUI consumers
should inspect and consume tickets each frame rather than wait for the entire
queue to finish. `clear_cache()` evicts current entries; active reads may refill
it after that call. Statistics expose reads, hits, payload usage and queue state.

## Cancellation and shutdown

`cancel(ticket)` suppresses publication immediately and releases ready pixels.
The underlying synchronous reader is not forcibly interrupted. Failed reads
publish an error string without pixels and release the reservation. Stale and
canceled reads do not populate the cache.

`shutdown()` rejects new work, cancels unconsumed tickets, clears the cache,
waits for active native reads and joins workers. It is idempotent and leaves
the shared image server open. Shut down the scheduler before explicitly closing
its server. Sharing a server with a document does not transfer close ownership.
Previously returned buffers remain valid after either object is destroyed.

Open/read/close server operations and blocking scheduler waits/shutdown release
the Python GIL. Native destruction may wait for a reader while holding the GIL;
workers execute native code only, so they do not require that GIL to finish.
An image decoder that hangs can delay shutdown; process isolation and hard read
timeouts are future work.

## Example

```python
import qupathxx as qpx

server = qpx.open_slide("sample.svs")
scheduler = qpx.TileScheduler(server)
try:
    ticket = scheduler.submit(qpx.RegionRequest(1000, 2000, 0, 256, 256), priority=10)
    # Suitable for one request. A viewer polls and consumes each frame instead.
    if scheduler.wait_idle(5):
        result = scheduler.take(ticket)
        if result.status == qpx.TileStatus.READY:
            rgba = result.pixels.data
finally:
    scheduler.shutdown()
    server.close()
```

## Acceptance evidence and next integration

Native tests gate a fake decoder with a condition variable to verify priority,
priority promotion, deduplication, active cancellation, queued stale rejection,
shutdown during reads, decode failures and work-budget retention without sleeps.
Python tests cover the installed API, shared buffer lifetimes, cache hits/LRU,
generation changes, ticket consumption, backpressure and invalid configuration.

Next, add viewport tile selection, generation-aware publication and a bounded
GPU texture cache to the ImGui viewer. Preserve main-thread OpenGL ownership.
Frame latency, DPI alignment and actual slide navigation remain unvalidated until
that integration is implemented and exercised.
