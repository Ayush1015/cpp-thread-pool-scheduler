# Verification record

Verified locally on October 1, 2026.

Environment: Linux x86_64, GCC 11.4.0, CMake 3.22.1, C++17, Catch2 2.13.10.

## Results for the final source

- Debug build: passed with `-Wall -Wextra -Wpedantic -Werror`.
- Catch2: 14 cases, 2,041 assertions, all passed.
- CTest: all 3 entries passed (Catch2 suite, demo, scripted CLI checks).
- Concurrency subset: 30 repeat runs passed; each includes a 2,000-job,
  eight-producer exactly-once test and a submission/shutdown race.
- ThreadSanitizer: final full suite passed 10 repeat runs, no sanitizer reports.
- Demo: five jobs accepted and completed; final queued=0, active=0, completed=5.
- GitHub Actions YAML: parsed and checked with actionlint 1.7.12, no findings.
- Hosted GitHub CI: not yet run at the time this local record was written.
  Do not turn local actionlint validation into a claim of a hosted CI pass.

An initial sanitizer run found warnings in the test-only four-worker gate using
an extra condition variable and counter. That gate was changed to an atomic
counter with a bounded timeout and yield, and all final sanitizer repeats passed.
No warning suppression was added. This record describes the final source only;
passing stress and sanitizer runs is not proof that every possible race is absent.

## Coverage

1. Invalid configuration.
2. Futures, return values, void jobs, move-only captures.
3. Exception propagation and worker survival.
4. Bounded queue overflow with a gated running task.
5. FIFO execution with a single worker.
6. Four workers execute concurrently.
7. Eight concurrent producers, 2,000 distinct jobs, exactly-once execution.
8. Draining shutdown, rejection afterwards, repeated shutdown.
9. Empty-pool shutdown.
10. Destructor draining.
11. Submission/shutdown race: all accepted work completes.
12. Concurrent shutdown callers.
13. Same-pool worker wait/shutdown rejected to avoid self-deadlock.
14. Consistent stats snapshots during concurrent execution.

CLI checks include success, wait/status, malformed duration, numeric overflow,
unexpected arguments, unknown commands, and invalid command-line options.

## Repeat it

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/scheduler_tests
cmake -S . -B build-tsan -DENABLE_TSAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tsan --parallel 2
./build-tsan/scheduler_tests
```
