# Vehicle ECU Simulator

## Overview
A modular Embedded C application simulating a simplified Vehicle ECU responsible for:
- Reading and validating vehicle inputs (speed, temperature, gear, mode)
- Executing control logic in a defined scheduler order
- Detecting and tracking faults using bitwise flags and counters
- Managing system state transitions (NORMAL → DEGRADED → SAFE)
- Generating structured system logs

## Project Structure

```
ECUDesignHackathon/
├── _objtmp/                # Temporary build artifacts
├── lib/                    # ECU module library
│   ├── control.c/h         # Overspeed, temperature, gear checks
│   ├── ecu_control.c       # Test runner / ECU controller
│   ├── fault.c/h           # Fault flags, counters, persistence
│   ├── input.c/h           # Input reading and validation
│   ├── log.c/h             # Structured console + file logging
│   ├── mode.c/h            # Mode controller (OFF/ACC/IGNITION_ON/FAULT)
│   ├── state.c/h           # System state manager (NORMAL/DEGRADED/SAFE)
│   ├── types.h             # Common enums, structs, macros
│   ├── libecu.a            # Static library (built by make)
│   └── Makefile            # Library sub-build
├── main.c                  # Scheduler loop and entry point
├── Makefile                # Root build system
├── test_cases.json         # Test case definitions
├── run.bat                 # Build + single run
├── runMul.bat              # Build + multiple runs (determinism check)
├── log.txt                 # Generated test output
├── performance_report.csv  # Cycle timing data
├── Readme.md               # This file
└── TEST_REPORT.md          # Test results report
```

## Scheduler Flow

Each ECU cycle executes in this **mandatory fixed order**:

1. **Clear Faults** — Reset per-cycle fault flags (counters persist)
2. **Read Inputs** — Get speed, temperature, gear, requested mode
3. **Validate Inputs** — Range-check; replace invalid with last valid value
4. **Update Mode** — Check transition legality; force FAULT on illegal
5. **Run Control Checks** — Detect overspeed, overtemp, invalid gear
6. **Update Fault Status** — Increment counters for active faults
7. **Evaluate System State** — Determine NORMAL / DEGRADED / SAFE
8. **Generate Logs** — Print complete cycle summary

### Why This Order Matters
- Inputs must be validated **before** any logic uses them
- Mode must be determined **before** control checks
- All fault detections must complete **before** counters are updated
- State evaluation must happen **after** counters reflect current cycle
- Logs must be **last** to capture the full cycle picture

## Module Responsibilities

| Module | File | Responsibility |
|--------|------|---------------|
| Input | `input.c/h` | Read raw inputs, validate ranges, preserve last valid |
| Mode | `mode.c/h` | Enforce mode transition rules using switch-case |
| Control | `control.c/h` | Evaluate overspeed, temperature, gear thresholds |
| Fault | `fault.c/h` | Bitwise fault flags, per-fault counters |
| State | `state.c/h` | NORMAL/DEGRADED/SAFE transitions, SAFE latch |
| Log | `log.c/h` | Priority-ordered console + file logging |
| ECU Control | `ecu_control.c` | Automated test execution for all 9 test cases |

## State Transition Logic

```
NORMAL ──(1 fault)──> DEGRADED ──(2+ faults / persistence)──> SAFE
  ^                      |                                        |
  |______(no faults)_____|                                        |
                                                                  |
  SAFE is LATCHED: Only recovers via MODE_OFF + no active faults  |
```

## Fault Handling Strategy

- **Bitwise flags**: Each fault type has a unique bit position (`uint32_t`)
- **Counters**: Per-fault `uint16_t` counters track persistence across cycles
- **Per-cycle clearing**: `active_faults` reset to 0 at cycle start; counters preserved
- **Extensibility**: New faults added by defining a new bit mask + counter field
- **Priority reporting**: Critical Overheat > Invalid Gear/Mode > Overspeed > High Temp

## Build and Run

```bash
# Build
mingw32-make

# Run (auto-test mode)
run.bat

# Multiple runs (determinism verification)
runMul.bat
```

## Test Cases

All 9 mandatory test cases from the hackathon specification are implemented.
See `TEST_REPORT.md` for detailed results.
