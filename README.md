# Thread Pool Job Scheduler

A working C++17 learning project: a bounded FIFO job queue, reusable worker threads,
a command-line demo, futures, graceful shutdown, and tests.

This is a user-space concurrency project, not an embedded/RTOS scheduler, kernel
driver, wireless stack, or production task service. CLI jobs simulate work with a
sleep; the library accepts real zero-argument callables and returns their results.

## Quick start on Linux / WSL

Requirements: GCC or Clang with C++17, CMake 3.16+, a build tool, and Bash for the
CLI integration test. On Ubuntu, install `build-essential cmake` if needed.
Catch2 v2.13.10 is included, so configuration needs no network connection.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/scheduler_cli --demo
./build/scheduler_cli --workers 3 --capacity 8
```

Paste these commands into the interactive CLI:

```text
submit parse-log 100
submit calculate 50
status
wait
status
quit
```

Each accepted job prints `ACCEPTED`, `START`, and `DONE`. Output lines do not
interleave. Jobs leave the queue in FIFO order, but with multiple workers their
START output and completion order can differ. `wait` blocks until idle, `status`
prints a consistent snapshot, and `quit` or end-of-input drains accepted jobs.
Queue-full submission returns `REJECTED` immediately; the user can retry later.
Duration must be 1..60000 milliseconds, and the name must be one word of at most
64 characters. Workers: 1..64. Pending queue capacity: 1..100000.

## Use the library

```cpp
#include "scheduler/thread_pool.hpp"

scheduler::ThreadPool pool(4, 64);
auto result = pool.submit([] { return 6 * 7; });
int answer = result.get(); // 42, or rethrows the job's exception
pool.shutdown();          // rejects new jobs, drains existing jobs, joins workers
```

The callable must take no arguments. Capture arguments in a lambda. Move-only
captures are supported. Catch `QueueFull` and `PoolStopped` when submitting.

## Architecture

```text
CLI or producer threads
        |
        v
 submit -> [mutex-protected bounded FIFO queue] -> worker threads -> future
                      |                               |
                 condition_variable             completion counters
```

- `include/scheduler/thread_pool.hpp`: public API, stats, exceptions, templated submit.
- `src/thread_pool.cpp`: queue synchronization, workers, idle barrier, shutdown.
- `src/main.cpp`: validated CLI input and synchronized logging.
- `tests/test_thread_pool.cpp`: 14 Catch2 cases including concurrency and shutdown races.
- `scripts/test_cli.sh`: scripted CLI success, invalid input, and invalid option checks.
- `.github/workflows/ci.yml`: GCC/Clang build and tests on `ubuntu-latest`.
- `WALKTHROUGH.md`: source-by-source explanation.
- `INTERVIEW_QA.md`: interview practice and honest attribution guidance.
- `TEST_RESULTS.md`: actual local verification, including limitations.

All queue state and counters share one mutex. Workers sleep on a condition
variable when idle. A predicate makes waiting safe against spurious wakeups.
User tasks execute outside the queue lock. A separate shutdown mutex serializes
joins, so two external shutdown callers do not join the same thread concurrently.

Accounting invariant: `submitted == queued + active + completed`. Completed
includes tasks that threw an exception; the exception is stored in the future.
Capacity bounds queued tasks only. Running tasks are bounded by worker count.
Total accepted but unfinished jobs are at most capacity plus workers.

## Safety contract and limits

- Keep the pool and any captured objects alive until all dependent jobs finish.
  Never destroy the pool from one of its own jobs. Destruction there is a contract
  violation that can terminate the process; shutdown/wait from a worker instead
  throw `logic_error` so those common self-deadlocks fail clearly.
- Do not have every worker wait on futures for child jobs submitted to this same
  pool. With no worker free to run the children, that creates starvation deadlock.
- A task that never returns makes draining shutdown wait forever. There is no
  cancellation, deadline enforcement, persistence, fairness guarantee, priority,
  or hard-real-time scheduling. Task process crashes are not contained.
- `wait_idle` is a snapshot barrier, not a producer stop signal. Concurrent
  producers may submit immediately after it returns.
- Thread-safe submission does not make arbitrary captured user data thread-safe.
- This is tested code, not a claim of bug-free software or production certification.

## Tests and optional race tooling

```sh
./build/scheduler_tests
./build/scheduler_tests '[concurrency]'
for i in $(seq 1 30); do ./build/scheduler_tests '[concurrency]' || exit 1; done

# Optional on a supported Linux host. Runtime support depends on the environment.
cmake -S . -B build-tsan -DENABLE_TSAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tsan --parallel 2
./build-tsan/scheduler_tests '[concurrency]'
```

Stress tests expose likely synchronization failures, but passing them is not a
proof that all races are absent. ThreadSanitizer helps detect races in executed
paths, and does not prove all paths safe either. See the recorded final checks
in `TEST_RESULTS.md` before describing sanitizer results to anyone.

## GitHub and resume use

Upload this folder's contents, including `.github`, not the `build` directories.
Run the commands yourself and confirm your own GitHub Actions run is green before
claiming a hosted CI result. The included workflow has been checked locally, but
has not yet run in your repository.

This starter was AI-assisted. Do not describe receiving it as independently
building it. Read the walkthrough, explain the locking yourself, and make and test
at least one meaningful change before presenting it as your project contribution.
Examples: add elapsed-time stats, a CLI command that runs a real file-processing
job, or cooperative cancellation with a documented safety contract. Keep a clear
record of what you personally changed.

## License

Project code and documentation: MIT (see `LICENSE`). Vendored Catch2 retains its
own Boost Software License in `third_party/Catch2-LICENSE.txt` and source header.
