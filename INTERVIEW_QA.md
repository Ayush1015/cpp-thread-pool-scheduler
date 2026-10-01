# Interview practice

Use these to check your understanding, not as a script. Run the project yourself
and keep examples of your own changes. Do not invent independent authorship,
benchmarks, production users, embedded experience, or a hosted CI pass.

## 1. What does the project do?
A C++17 user-space thread pool executes accepted callables using a bounded FIFO
queue. A CLI simulates jobs. Discuss fixed workers, explicit overload rejection,
futures, and draining shutdown. It is not an OS process scheduler.

## 2. Why a pool rather than one thread per job?
A fixed set reuses threads and limits concurrent execution. Per-job threads can
cause creation overhead and resource growth. We did not benchmark a speedup, so
do not give a performance number. Whether it is faster depends on workload.

## 3. Why mutex plus condition_variable?
The mutex protects shared state; the condition variable lets workers sleep until
there is work or shutdown. Explain why wait releases the mutex and reacquires it,
and why the predicate is checked even after a notification.

## 4. Where could races happen?
Queue push/pop, the accepting flag, and counters. All share one mutex. User code
can still race on its captures. Explain the difference between C++ data races
(undefined behavior) and broader logical races. Show the exact-once stress test.

## 5. What does bounded mean, and how is overload handled?
Only pending jobs count toward capacity. Running jobs count toward worker count.
Submission refuses a full queue with QueueFull and never silently drops a job.
The upper bound on accepted unfinished work is capacity plus workers.

## 6. How do exceptions and return values work?
A packaged_task stores the callable's return value or exception. submit returns
a future, and get waits then returns or rethrows. A task exception does not kill
the worker. Rejection during submission is separate from a task failure.

## 7. How does shutdown avoid lost work or deadlock?
Stop accepting under the same mutex used by submit, wake workers, drain the
queue, and join without holding the queue mutex. A separate lock serializes
concurrent joins. Same-pool worker shutdown/wait calls throw. Walk through both
possible outcomes of submit racing with shutdown.

## 8. Is FIFO guaranteed?
Dequeue order is FIFO. Multiple workers may start or finish in different orders
because the OS schedules them independently. One worker makes execution order
FIFO. Do not claim priority, fairness, latency bounds, or real-time behavior.

## 9. How did you test it? Is it race-free?
Describe 14 Catch2 cases, CLI integration, 2,000-job exact-once stress, bounds,
shutdown races, exception propagation, and repeat runs. Passing stress tests is
not proof of no races. Report only your actual local and CI/sanitizer results.
The final full suite passed 10 repeated ThreadSanitizer runs without reports.
That checks executed paths, not every possible interleaving.

## 10. What are the limits and next improvements?
No cancellation or persistence; long or stuck jobs delay shutdown. Child jobs
waiting on the same saturated pool can deadlock. Captured object lifetimes are
the caller's responsibility. Good extensions: measured queue depth, real file
jobs, cooperative cancellation, and benchmarks with a clearly defined workload.
Say which one you personally implemented, rather than implying all exist.

## Honest authorship answer
"I started from an AI-assisted implementation. I ran and studied it, then I
implemented [your actual change] and tested it with [your actual tests]. I can
explain the queue locking, future handling, and shutdown tradeoffs."

If you have not done those steps, say so. A repository upload alone does not
create hands-on concurrency experience. A defensible resume line comes after
your contribution, for example:

"Extended an AI-assisted C++17 thread-pool scheduler with [real feature]; tested
bounded queuing and draining shutdown using Catch2."
