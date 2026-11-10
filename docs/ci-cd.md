# CI and release delivery

The [workflow](../.github/workflows/ci.yml) follows the QCD project's CMake preset
approach. It runs on pull requests, pushes to `master`, version tags (`v*`), and
manual runs from the Actions tab.

## Checks

| Check | Compiler | Purpose |
| --- | --- | --- |
| GCC Debug | GCC 13 | Full CTest suite without optimizations |
| GCC Release | GCC 13 | Full suite with optimizations, then real-service smoke test |
| Clang Debug | Clang 18 | Compiler portability |
| Address / UB Sanitizers | Clang 18 | Memory errors, leaks, and undefined behavior |
| ThreadSanitizer | Clang 18 | Data races, including existing concurrency and WebSocket tests |
| Coverage | GCC 13 / gcovr | Source line coverage in logs and the run summary |

`CI passed` succeeds only when all builds and coverage succeed; it is the single
check to select if you later configure branch protection. Each CTest case has a
30-second limit, and builds have a 20-minute job limit. Empty test suites fail.
An updated branch cancels its older CI run. Tag runs finish without cancellation.
No coverage percentage is enforced until a baseline is agreed on.

The smoke test launches the built executable, waits up to 15 seconds for prices,
validates price and order-book responses, checks URL-encoded symbols and a missing
symbol's HTTP 404, and requires clean shutdown on SIGTERM. It needs port 8080 free.

ThreadSanitizer's job sets `vm.mmap_rnd_bits=28` on its disposable runner before
building. This avoids sanitizer startup failures from incompatible address-space
layouts. The workflow does not change your workstation's settings. See the
[sanitizer maintainers' explanation](https://github.com/google/sanitizers/issues/1614).
Sanitizer findings fail CI; no reports are suppressed or ignored.

## Cost

The repository is public. GitHub documents that
[standard GitHub-hosted runners are free in public repositories](https://docs.github.com/en/billing/concepts/product-billing/github-actions).
Every job includes a public-repository condition, so these jobs are skipped if the
repository becomes private. There are no paid runners, cloud resources, scheduled
jobs, Actions cache uploads, or Actions artifact uploads. Coverage stays in logs
and the job summary rather than accumulating stored artifacts.

Versioned binaries are delivered through GitHub Releases. GitHub documents
[no total release storage or bandwidth limit](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases),
with a 2 GiB per-file limit. The pipeline does not create an always-running server;
downloaded binaries run on your own machine. No AWS, Azure, Docker Hub, or other
external deployment account is required.

## Local checks

Requirements: CMake 3.28+, Ninja, Boost headers (1.75+), a C++20 compiler, and
Python 3 for the smoke test. CI uses Ubuntu 24.04, GCC 13, and Clang 18 explicitly.
GoogleTest is downloaded at the commit already pinned in `test/CMakeLists.txt`.

```bash
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug --timeout 30 --no-tests=error

cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release --timeout 30 --no-tests=error
python3 scripts/smoke-test.py build/release/market_data_service
```

Use `asan` or `tsan` in place of `debug` for sanitizer builds. To use Clang in a
fresh preset build directory, prefix configure with `CC=clang CXX=clang++`.
Configure-time compiler selection is cached by CMake; use separate build
directories when switching compilers.

For coverage, install `gcovr`, then use GCC and a matching `gcov` version:

```bash
cmake --preset coverage
cmake --build --preset coverage --parallel 2
ctest --preset coverage --timeout 30 --no-tests=error
gcovr --root . --filter 'src/' --txt --print-summary build/coverage
```

## Publish a release

Commit and push the workflow first. From the commit you want to release:

```bash
git tag v1.0.0
git push origin v1.0.0
```

All CI checks must pass before the release job starts. It rebuilds and retests
Release, smoke-tests the service, and publishes:

- `exchange-engine-v1.0.0-linux-x86_64.tar.gz`
- `SHA256SUMS`

The archive contains the executable, project README, runtime dependency listing,
and run instructions. Only the release job receives `contents: write`, using
GitHub's automatic token. It needs no repository secrets. A `-` suffix such as
`v1.0.0-rc.1` marks the release as a prerelease. Re-running an existing published
version leaves its assets unchanged; publish a new version to change binaries.

Verify and run a downloaded release:

```bash
sha256sum --check SHA256SUMS
tar -xzf exchange-engine-v1.0.0-linux-x86_64.tar.gz
./exchange-engine-v1.0.0-linux-x86_64/market_data_service
```

These binaries target Linux x86-64 with Ubuntu 24.04-compatible glibc and C++
runtimes. They are dynamically linked; other platforms should build from source.
No container image or remote service is deployed.
