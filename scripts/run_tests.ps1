<#
.SYNOPSIS
    EZ language test runner.

.DESCRIPTION
    Discovers the .ez tests under Test/, runs each one in its own process
    SYNCHRONOUSLY (with a timeout), captures stdout/stderr and the exit code,
    and classifies the result.

    This replaces the previous runner, which shelled out via WinExec() --
    fire-and-forget, so it could never observe whether a test passed.

    A test FAILS if any of these hold:
      * it exits with a non-zero status,
      * it times out,
      * its output contains a failure marker (FAIL / TEST FAILED / "Fails: N"
        with N > 0 / an "Error:"/"Type Error" diagnostic).
    Otherwise it PASSES.

    Tests listed in $KnownFailures are pre-existing, tracked failures: they are
    reported as KNOWN-FAIL and do not break the build. A known-failing test that
    starts passing is reported as FIXED so the list can be pruned.

.PARAMETER Filter
    Only run tests whose name matches this wildcard (e.g. -Filter 'test_gc*').

.PARAMETER IncludeHandwritten
    Also run the ~100 small cases in Test/HandwrittenTests.

.PARAMETER TimeoutSec
    Per-test timeout. Default 60.

.PARAMETER Interpreter
    Path to the ez.exe to test. Defaults to build\ez.exe, then ez.exe. Useful for
    comparing a new build against a previous one to separate genuine regressions
    from pre-existing failures.

.PARAMETER IgnoreKnown
    Report known failures as ordinary FAILs (used when baselining).

.EXAMPLE
    .\scripts\run_tests.ps1
    .\scripts\run_tests.ps1 -Filter 'test_gc*' -TimeoutSec 30
    .\scripts\run_tests.ps1 -IncludeHandwritten
    .\scripts\run_tests.ps1 -Interpreter .\ez.exe -IgnoreKnown
#>
[CmdletBinding()]
param(
    [string] $Filter = '*',
    [switch] $IncludeHandwritten,
    [int]    $TimeoutSec = 60,
    [string] $Interpreter = '',
    [switch] $IgnoreKnown
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

# ── Locate the interpreter ───────────────────────────────────────────────────
$candidates = if ($Interpreter) { @((Resolve-Path -LiteralPath $Interpreter -ErrorAction SilentlyContinue).Path) }
              else { @(
                  (Join-Path $repoRoot 'build\ez.exe'),
                  (Join-Path $repoRoot 'ez.exe')
              ) }
$ez = $candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if (-not $ez) {
    Write-Host "ERROR: no interpreter found. Looked for:" -ForegroundColor Red
    $candidates | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    Write-Host "Build it first (build.bat or build_cmake.bat)." -ForegroundColor Red
    exit 2
}

# ── Tests that are not standalone pass/fail tests ────────────────────────────
# Modules imported by other tests, plus tests needing a GUI/server/human.
$Excluded = @(
    'lib_a.ez', 'lib_b.ez', 'error_module.ez',   # modules `use`d by test_modules.ez
    'test_gui_native.ez',                        # opens a window
    'test_server.ez',                            # binds a socket and blocks
    'test_notify.ez',                            # OS notification popup
    'tempCodeRunnerFile.ez',                     # editor scratch file
    'test.ez',                                   # scratch pad, not a test: prints a
                                                 # name then does `z+=1` on an
                                                 # undeclared variable
    'test_suite_demo.ez',                        # a DEMO of the unittest lib whose
                                                 # last case asserts 1+1==3 on purpose
                                                 # ("Intentional failure") to show what
                                                 # a failure looks like
    'bench_dict.ez'                              # benchmark: prints timings, asserts
                                                 # nothing
)

# ── Pre-existing failures (tracked, do not break the build) ──────────────────
# Add an entry here only for a failure that is understood and deliberately
# deferred; a quarantined test that starts passing is reported as FIXED so this
# list does not rot.
#
# Currently EMPTY: the whole suite passes. Everything that used to live here was
# either a real defect (TCO never reusing frames; typeOf() reporting "unknown"
# for short/concatenated strings, which silently stopped lib/db.ez binding text
# parameters; `async { }` blocks documented but never parsed) or a test asserting
# something the language deliberately does not do (spawn snapshots captured
# upvalues, so shared state must be an instance/array/dict).
$KnownFailures = @{}

# ── Discover tests ───────────────────────────────────────────────────────────
$tests = @(Get-ChildItem -Path (Join-Path $repoRoot 'Test') -Filter '*.ez' -File)
if ($IncludeHandwritten) {
    $hw = Join-Path $repoRoot 'Test\HandwrittenTests'
    if (Test-Path $hw) {
        $tests += @(Get-ChildItem -Path $hw -Filter '*.ez' -File)
    }
}
$tests = $tests |
    Where-Object { $Excluded -notcontains $_.Name } |
    Where-Object { $_.Name -like $Filter } |
    Sort-Object Name

if (-not $tests) {
    Write-Host "No tests matched filter '$Filter'." -ForegroundColor Yellow
    exit 0
}

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ' EZ Test Suite' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host " interpreter : $ez"
Write-Host " tests       : $($tests.Count)"
Write-Host " timeout     : ${TimeoutSec}s per test"
Write-Host ''

# Markers that indicate a failure even when the process exits 0.
# Deliberately NARROW: the exit code is the reliable signal (EZ exits 65 on
# compile/type errors, 70 on uncaught runtime errors), so this only needs to
# catch tests that self-report a failure while still exiting 0. Do not match
# bare "Error:" / "Type Error" here -- error-handling tests legitimately print
# those as expected output.
$failurePattern = '(?m)(^|\s)(FAIL\b|!!! TEST FAILED|Fails:\s*[1-9]|\[FATAL\])'

$pass = 0; $fail = 0; $knownFail = 0; $fixed = 0
$failedNames = @()
$fixedNames  = @()
# name -> everything the test printed, for the failure report at the end.
$failureOutput = @{}

# GitHub renders ::group:: as a collapsible section and ::error:: as an
# annotation on the job summary and the PR diff. Outside Actions these would be
# noise, so they are only emitted when actually running there.
$inActions = [bool]$env:GITHUB_ACTIONS

foreach ($t in $tests) {
    $name = $t.Name
    $outFile = [System.IO.Path]::GetTempFileName()
    $errFile = [System.IO.Path]::GetTempFileName()
    $status  = 'PASS'
    $reason  = ''

    # A test may declare that it is SUPPOSED to be rejected at compile time, with
    #     # EXPECT: compile-error
    # on any of its first few lines. Some behaviour is only observable as a
    # compile error -- an out-of-scope variable, say -- because the type checker
    # rejects the program before anything can run, so a try/catch around it never
    # executes. Those tests previously looked like failures while actually
    # demonstrating that the checker works.
    $expectCompileError = $false
    $head = Get-Content $t.FullName -TotalCount 5 -ErrorAction SilentlyContinue
    if ($head -match '^\s*(#|//)\s*EXPECT:\s*compile-error') { $expectCompileError = $true }

    # Whatever the test printed. Kept even on the timeout path, because the
    # output up to the point it wedged is usually what identifies where.
    $output = ''

    try {
        $proc = Start-Process -FilePath $ez -ArgumentList $t.FullName `
                              -NoNewWindow -PassThru `
                              -RedirectStandardOutput $outFile `
                              -RedirectStandardError  $errFile `
                              -WorkingDirectory $repoRoot

        if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
            try { $proc.Kill() } catch {}
            $status = 'FAIL'; $reason = "timed out after ${TimeoutSec}s"
            Start-Sleep -Milliseconds 100   # let the killed process flush
            if (Test-Path $outFile) { $output += (Get-Content $outFile -Raw -ErrorAction SilentlyContinue) }
            if (Test-Path $errFile) { $output += (Get-Content $errFile -Raw -ErrorAction SilentlyContinue) }
        }
        else {
            if (Test-Path $outFile) { $output += (Get-Content $outFile -Raw -ErrorAction SilentlyContinue) }
            if (Test-Path $errFile) { $output += (Get-Content $errFile -Raw -ErrorAction SilentlyContinue) }

            if ($expectCompileError) {
                # 65 is the compile/type-error exit. Anything else means the
                # program was accepted (or died some other way), which is the
                # failure this test exists to catch.
                if ($proc.ExitCode -eq 65) {
                    $status = 'PASS'
                } else {
                    $status = 'FAIL'; $reason = "expected a compile error (exit 65), got exit $($proc.ExitCode)"
                }
            }
            elseif ($proc.ExitCode -ne 0) {
                $status = 'FAIL'; $reason = "exit code $($proc.ExitCode)"
            }
            elseif ($output -match $failurePattern) {
                $status = 'FAIL'; $reason = "failure marker: $($Matches[0].Trim())"
            }
        }
    }
    catch {
        $status = 'FAIL'; $reason = $_.Exception.Message
    }
    finally {
        Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue
    }

    $isKnown = (-not $IgnoreKnown) -and $KnownFailures.ContainsKey($name)

    if ($status -eq 'PASS' -and $isKnown) {
        $fixed++; $fixedNames += $name
        Write-Host ('  [FIXED]      {0}' -f $name) -ForegroundColor Magenta
        Write-Host ('               was: {0}' -f $KnownFailures[$name]) -ForegroundColor DarkGray
    }
    elseif ($status -eq 'PASS') {
        $pass++
        Write-Host ('  [PASS]       {0}' -f $name) -ForegroundColor Green
    }
    elseif ($isKnown) {
        $knownFail++
        Write-Host ('  [KNOWN-FAIL] {0}' -f $name) -ForegroundColor DarkYellow
        Write-Host ('               {0}' -f $KnownFailures[$name]) -ForegroundColor DarkGray
    }
    else {
        $fail++; $failedNames += $name
        Write-Host ('  [FAIL]       {0}  ({1})' -f $name, $reason) -ForegroundColor Red

        # Show what the test actually printed.
        #
        # The reason alone is frequently useless: a test that wraps its whole
        # body in one try/catch reports "failure marker: !!! TEST FAILED" and
        # nothing else, so the exception text -- the one line that says which
        # assertion broke -- was captured, then deleted with the temp files.
        # Diagnosing meant re-running by hand, which is no help at all when the
        # failure happened on a CI runner.
        $failureOutput[$name] = $output
    }
}

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ' Summary' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ("  Passed      : {0}" -f $pass)      -ForegroundColor Green
Write-Host ("  Failed      : {0}" -f $fail)      -ForegroundColor ($(if ($fail) { 'Red' } else { 'Green' }))
Write-Host ("  Known-fail  : {0}" -f $knownFail) -ForegroundColor DarkYellow
if ($fixed) {
    Write-Host ("  Fixed       : {0}  (remove from `$KnownFailures)" -f $fixed) -ForegroundColor Magenta
    $fixedNames | ForEach-Object { Write-Host "      $_" -ForegroundColor Magenta }
}
if ($fail) {
    Write-Host ''
    Write-Host '  Failing tests:' -ForegroundColor Red
    $failedNames | ForEach-Object { Write-Host "      $_" -ForegroundColor Red }

    # Full output for each failure, so a CI log is enough to diagnose from
    # without reproducing locally.
    Write-Host ''
    Write-Host '========================================' -ForegroundColor Red
    Write-Host ' Failure details' -ForegroundColor Red
    Write-Host '========================================' -ForegroundColor Red

    foreach ($n in $failedNames) {
        $body = $failureOutput[$n]
        if ([string]::IsNullOrWhiteSpace($body)) {
            $body = '(the test produced no output -- it most likely died before printing anything)'
        }
        $body = $body.TrimEnd()

        if ($inActions) { Write-Host "::group::FAILED $n" }
        Write-Host ''
        Write-Host ("--- $n " + ('-' * [Math]::Max(0, 56 - $n.Length))) -ForegroundColor Red
        $body -split "`r?`n" | ForEach-Object { Write-Host "  $_" }
        if ($inActions) { Write-Host '::endgroup::' }

        if ($inActions) {
            # One annotation per failure, pinned to the file so it shows up in
            # the PR diff. Newlines have to be encoded or Actions truncates the
            # message at the first one.
            $firstError = ($body -split "`r?`n" |
                           Where-Object { $_ -match 'Error|FAIL|Exception|Traceback' } |
                           Select-Object -First 1)
            if (-not $firstError) { $firstError = $body -split "`r?`n" | Select-Object -Last 1 }
            $encoded = ($body -replace '%', '%25' -replace "`r", '' -replace "`n", '%0A')
            $rel = "Test/$n"
            Write-Host "::error file=$rel,title=$n::$firstError%0A%0A$encoded"
        }
    }
}
Write-Host ''

exit $(if ($fail -gt 0) { 1 } else { 0 })
