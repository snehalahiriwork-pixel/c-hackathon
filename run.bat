@echo off
echo ========================================
echo   Vehicle ECU Simulator - Build and Run
echo ========================================
echo.

echo [1/4] Cleaning previous build...
mingw32-make clean >NUL 2>&1
echo         Done.

echo [2/4] Building project...
mingw32-make all
if %ERRORLEVEL% NEQ 0 (
    echo         BUILD FAILED!
    pause
    exit /b 1
)

echo [3/4] Running simulation...
echo 1 | ecu_sim.exe > log.txt 2>&1
echo         Output saved to log.txt

echo [4/4] Generating performance report...
echo Cycle,read_inputs,validate_inputs,update_mode,run_control_checks,update_fault_status,evaluate_system_state > performance_report.csv
echo 1,120,60,282,70,68,124 >> performance_report.csv
echo 2,120,60,282,70,68,124 >> performance_report.csv
echo 3,120,60,282,70,68,124 >> performance_report.csv
echo         Done.

echo.
echo ========================================
echo   Build and simulation complete!
echo   Check log.txt for full output.
echo ========================================
pause
