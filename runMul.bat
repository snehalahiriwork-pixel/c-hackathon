@echo off
echo ========================================
echo   Vehicle ECU Simulator - Multiple Runs
echo ========================================
echo.

echo [1/4] Building project...
mingw32-make all
if %ERRORLEVEL% NEQ 0 (
    echo         BUILD FAILED!
    pause
    exit /b 1
)

echo [2/4] Running all 9 test cases (Run 1)...
echo 1 | ecu_sim.exe > log.txt 2>&1
echo         Run 1 complete - output in log.txt

echo [3/4] Running all 9 test cases (Run 2 - verification)...
echo 1 | ecu_sim.exe > _tmp_dump.txt 2>&1
echo         Run 2 complete - output in _tmp_dump.txt

echo [4/4] Comparing outputs for determinism...
fc log.txt _tmp_dump.txt >NUL 2>&1
if %ERRORLEVEL% EQU 0 (
    echo         PASS: Outputs are identical (deterministic execution confirmed)
) else (
    echo         WARN: Outputs differ (non-deterministic behaviour detected)
)

echo.
echo ========================================
echo   Multiple runs complete!
echo ========================================
pause
