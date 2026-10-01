# How this project works

Read this alongside the source. You should be able to draw the queue, workers,
and lock without memorizing these words.

## 1. Start with the problem

Creating a new OS thread for every small job has a cost. A thread pool starts a
fixed number of threads once, then reuses them. The queue holds jobs until a
worker is free. A bounded queue refuses excess pending work rather than growing
without limit. Here refusal is explicit, not silent dropping or blocking.

Try `--workers 1 --capacity 1`, submit a long job, then several short jobs. The
running job does not count against queue capacity. One pending job fits; another
is refused while that slot is occupied. Exact timing can vary, so the automated
bounds test uses a gate rather than guessing with sleeps.

## 2. The header defines the promise to callers

`ThreadPool` cannot be copied or moved because its threads refer to this object's
state. Copying or moving that state would break those references.

`submit` accepts a callable, such as a lambda, with no arguments. The compiler
works out its return type with `invoke_result_t`. `packaged_task` wraps the
callable and stores its return value or exception. A `future` lets the caller get
that outcome later. The shared pointer makes the queue wrapper copyable for
`std::function`, while the actual callable can own a move-only resource.

Making a future does not mean the queue accepted the job. `enqueue` can throw
`QueueFull` or `PoolStopped`; in that case nothing was submitted and the caller
must handle rejection. Jobs throwing during execution are a different case:
submission succeeded, and `future.get()` later rethrows their exception.

## 3. What the mutex protects

Open `thread_pool.cpp`. `mutex_` protects the queue, accepting flag, and four
counters. Multiple producers and workers may reach them together, so access must
be serialized. `stats()` uses that same lock to return one consistent snapshot.

A race condition here could mean two workers trying to remove the same front
job, producers changing queue storage concurrently, or updates to counters being
lost. A C++ data race is specifically conflicting accesses from threads without
proper synchronization, at least one being a write; it causes undefined behavior.
A logical race can also occur without a data race, for example accepting a job
at an unintended point during shutdown. This implementation uses one lock to
make the accept-or-reject decision atomic with shutdown changing the flag.

The lock is held only while editing bookkeeping. It is released before running
a job. Holding it through a slow job would stop producers and other workers.

## 4. Why use condition_variable?

Without a condition variable, an idle worker might repeatedly ask whether the
queue has work. That busy loop wastes CPU. `work_available_.wait` releases the
mutex and sleeps. Submission wakes one worker. Shutdown wakes all workers.

The wait uses `!queue_.empty() || !accepting_` as its predicate. On waking it
reacquires the mutex and checks again. This matters because wakeups can be
spurious, or another worker may already have taken the available job.
Notifications are not stored messages. The protected predicate is the truth:
a worker arriving after a notification still sees the nonempty queue or closed
flag and does not sleep incorrectly.

## 5. Follow a job through the system

1. `submit` wraps it in a packaged task and obtains its future.
2. `enqueue` locks, checks the flag and queue capacity, pushes it, increments
   submitted, unlocks, and notifies a worker.
3. A worker locks, removes the oldest queued job, increments active, and unlocks.
4. It runs the callable without the queue mutex. The packaged task captures errors.
5. It locks again, decreases active, increases completed, and wakes idle waiters
   if there is no queued or running work.

The future can become ready before step 5. If you need final accounting, call
`wait_idle` before inspecting final stats. A snapshot during execution need not
show all futures' outcomes as completed yet.

FIFO means removal order. Two workers can remove A then B, but OS scheduling may
let B print START first. Completion also depends on duration and scheduling.
There is no priority or hard-real-time guarantee.

## 6. Graceful shutdown

`shutdown` first checks that the caller is not a worker of this same pool, then
locks `shutdown_mutex_`. This separate lock allows only one join sequence at a
time. It sets accepting to false under the queue mutex and notifies all workers.

Workers still take accepted jobs until the queue is empty. Once the queue is
empty and accepting is false, they exit. `join()` waits for each OS thread to
finish. No queue lock is held while joining. Repeating shutdown is safe because
already joined threads are not joined again. The destructor calls shutdown.

The ordering of the self-check is intentional. If an owner were joining a worker
while holding shutdown_mutex_, and that worker first tried to lock it, both
could wait forever. Checking worker identity before locking avoids that path.
A thread-local pointer records which pool, if any, owns the current worker.

## 7. How deadlock is avoided, and where you can still cause one

- Jobs run outside the queue mutex.
- Condition-variable waiting releases the queue mutex while asleep.
- Joining does not hold the queue mutex that finishing workers need.
- Same-pool workers cannot call wait_idle or shutdown; they get logic_error.
- No code holds the queue mutex then tries to acquire shutdown_mutex_.

However, if all workers submit child jobs and then wait on their futures, no
worker remains to run those children. Do not use that pattern. Also do not destroy
the pool inside its worker, and do not submit jobs that depend on objects that
will be destroyed too early. Locks cannot fix an invalid lifetime contract.

## 8. The CLI is a demonstration, not the whole library

`main.cpp` validates options and commands, then submits lambdas that sleep.
`output_mutex` stops threads from mixing parts of console lines. Submission holds
that output lock until ACCEPTED prints, so a worker cannot print START first for
that job. The queue lock and output lock do not form an inversion: workers
release the queue lock before attempting console output.

`wait` waits for all current work, `status` shows counters, and `quit` drains.
You can replace the sleep lambda with a checksum calculation or file parser to
perform useful work. Check the lifetime and thread safety of any shared data.

## 9. How the tests avoid fake confidence

The bounds test starts one task behind a promise gate. Only after it has started
does the test fill the queue. This tests actual queued capacity without depending
on a sleep duration. It always releases the gate before demanding final results.

The multi-worker test waits for four tasks to reach a gate together. With only
one effective worker it cannot satisfy that condition; a timeout prevents a
permanent hang. The test then releases all jobs and verifies completion.

Eight producer threads submit 2,000 uniquely identified jobs. Atomic counters
record how often each runs. All must be exactly one, detecting duplicates and
lost jobs. Another test races shutdown against producers and checks that every
accepted task executes, while other attempts are explicitly rejected.

These are contention/race stress tests, not a mathematical proof of race freedom.
ThreadSanitizer is optional; only claim a sanitizer pass if it actually ran on
your machine without reports. See TEST_RESULTS.md for what was verified here.

## 10. Learn it by changing it

Run the demo and tests first. Then add one useful feature and its tests. A good
first feature is maximum observed queue depth: update it under mutex_, expose it
in Stats, and use the gated capacity test to verify it. Next implement a real
CLI job such as counting words in a file, with clear error handling and tests.

Explain your own changes in your repository. Receiving generated code is not
evidence that you already understand C++ concurrency. Understanding, modifying,
and testing it gives you an honest contribution you can discuss.
