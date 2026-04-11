# Vehicle ECU Simulator — Test Report

## Test Environment
- **Compiler**: GCC (MinGW) with `-Wall -Wextra -std=c99 -pedantic -O2`
- **Platform**: Windows
- **Build**: Static library `libecu.a` linked with `main.c`

---

## Test Case 1: Normal Start

| Field | Value |
|-------|-------|
| **Input** | Mode: OFF → ACC → IGNITION_ON, Speed=40, Temp=80, Gear=1 |
| **Expected** | No faults, NORMAL state throughout |
| **Actual** | Faults=0x00000000, State=NORMAL across all 3 cycles |
| **Result** | ✅ **PASS** |

---

## Test Case 2: Overspeed

| Field | Value |
|-------|-------|
| **Input** | Speed=130, Temp=85, Gear=4, Mode=IGNITION_ON |
| **Expected** | Overspeed detected, fault counter incremented, DEGRADED state |
| **Actual** | Faults=0x00000001 (OVERSPEED), OS counter=1, State=DEGRADED |
| **Result** | ✅ **PASS** |

---

## Test Case 3: High Temperature

| Field | Value |
|-------|-------|
| **Input** | Temp=100, Speed=40, Gear=1, Mode=IGNITION_ON |
| **Expected** | High temperature warning (PRIORITY 4), no crash |
| **Actual** | Faults=0x00000004 (OVERTEMP_HIGH), HighT counter=1, DEGRADED |
| **Result** | ✅ **PASS** |

---

## Test Case 4: Critical Overheat

| Field | Value |
|-------|-------|
| **Input** | Temp=115, Speed=40, Gear=1, Mode=IGNITION_ON |
| **Expected** | Critical fault (PRIORITY 1), state escalation, reason logged |
| **Actual** | Faults=0x00000002 (OVERTEMP_CRITICAL), CritT counter=1, DEGRADED, reason: "one active fault" |
| **Result** | ✅ **PASS** |

---

## Test Case 5: Invalid Gear

| Field | Value |
|-------|-------|
| **Input** | Gear=9, Speed=40, Temp=80, Mode=IGNITION_ON |
| **Expected** | Invalid gear fault, safe handling (gear corrected to last valid) |
| **Actual** | Faults=0x00000008 (INVALID_GEAR), InvGear counter=1, Gear corrected to 1, DEGRADED |
| **Result** | ✅ **PASS** |

---

## Test Case 6: Illegal Mode Transition

| Field | Value |
|-------|-------|
| **Input** | OFF → IGNITION_ON directly (skipping ACC) |
| **Expected** | Illegal transition detected, FAULT mode forced |
| **Actual** | Faults=0x00000010 (ILLEGAL_MODE), Mode=FAULT, IllMode counter=1, DEGRADED |
| **Result** | ✅ **PASS** |

---

## Test Case 7: Multiple Faults in One Cycle

| Field | Value |
|-------|-------|
| **Input** | Speed=140, Temp=120, Gear=9, Mode=IGNITION_ON |
| **Expected** | Multiple faults set, priority order respected, state escalated |
| **Actual** | Faults=0x0000000B (OVERSPEED+OVERTEMP_CRITICAL+INVALID_GEAR), 3 faults total, SAFE state |
| **Priority** | 1: CRITICAL OVERHEAT → 2: INVALID GEAR → 3: OVERSPEED |
| **Result** | ✅ **PASS** |

---

## Test Case 8: Persistent Fault Across Cycles

| Field | Value |
|-------|-------|
| **Input** | Speed=130, Temp=115, Gear=1, Mode=IGNITION_ON × 3 cycles |
| **Expected** | Fault counters increase, persistent behaviour visible, SAFE reached |
| **Actual** | Cycle 1: OS=1, CritT=1 → SAFE. Cycle 2: OS=2, CritT=2 → SAFE. Cycle 3: OS=3, CritT=3 → SAFE |
| **Result** | ✅ **PASS** |

---

## Test Case 9: Recovery / Reset Logic

| Field | Value |
|-------|-------|
| **Input** | Fault cycle (speed=130 → overspeed), then normal values (speed=40) |
| **Expected** | Recovery from DEGRADED to NORMAL when faults clear |
| **Actual** | Cycle 3: DEGRADED (overspeed). Cycle 4: NORMAL (no faults). Cycle 5: NORMAL confirmed |
| **Recovery Rule** | When all inputs return to valid ranges and no faults are active, system recovers to NORMAL |
| **Result** | ✅ **PASS** |

---

## Summary

| Test | Description | Status |
|------|-------------|--------|
| 1 | Normal Start | ✅ PASS |
| 2 | Overspeed | ✅ PASS |
| 3 | High Temperature | ✅ PASS |
| 4 | Critical Overheat | ✅ PASS |
| 5 | Invalid Gear | ✅ PASS |
| 6 | Illegal Mode Transition | ✅ PASS |
| 7 | Multiple Faults in One Cycle | ✅ PASS |
| 8 | Persistent Fault Across Cycles | ✅ PASS |
| 9 | Recovery / Reset Logic | ✅ PASS |

**Result: 9/9 test cases passed.**
