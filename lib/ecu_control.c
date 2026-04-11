/**
 * @file ecu_control.c
 * @brief ECU test controller — automated test runner with JSON-based loading and CPU profiling.
 */

#include "types.h"
#include "input.h"
#include "mode.h"
#include "control.h"
#include "fault.h"
#include "state.h"
#include "log.h"
#include "perf.h"

#define JSMN_STATIC
#include "jsmn.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ================================================================
 * Internal Configuration
 * ================================================================ */

#define MAX_TESTS           20U
#define MAX_CYCLES_PER_TEST 10U
#define JSON_BUFFER_SIZE    32768U
#define MAX_TOKENS          2048U

typedef struct {
    char           name[64];
    char           expected_desc[128];
    uint16_t       num_cycles;
    VehicleInput   cycles[MAX_CYCLES_PER_TEST];
    
    /* Validation criteria */
    Mode           exp_mode;
    SystemState    exp_state;
    FaultFlags     exp_faults;
} TestDefinition;

static TestDefinition g_tests[MAX_TESTS];
static uint16_t g_num_loaded_tests = 0;

/* ================================================================
 * Console display helpers
 * ================================================================ */

static const char* mode_to_name(Mode m)
{
    switch (m) {
        case MODE_OFF:         return "OFF";
        case MODE_ACC:         return "ACC";
        case MODE_IGNITION_ON: return "IGNITION_ON";
        case MODE_FAULT:       return "FAULT";
        default:               return "UNKNOWN";
    }
}

static const char* state_to_name(SystemState s)
{
    switch (s) {
        case STATE_NORMAL:   return "NORMAL";
        case STATE_DEGRADED: return "DEGRADED";
        case STATE_SAFE:     return "SAFE";
        default:             return "UNKNOWN";
    }
}

static void print_faults_inline_to_file(FILE *f, FaultFlags flags)
{
    if (flags == 0U) {
        fprintf(f, "NONE");
        return;
    }
    int first = 1;
    if (flags & FAULT_OVERTEMP_CRITICAL) { fprintf(f, "%sCRIT_OVERHEAT",  first ? "" : ", "); first = 0; }
    if (flags & FAULT_INVALID_GEAR)      { fprintf(f, "%sINVALID_GEAR",   first ? "" : ", "); first = 0; }
    if (flags & FAULT_ILLEGAL_MODE)      { fprintf(f, "%sILLEGAL_MODE",   first ? "" : ", "); first = 0; }
    if (flags & FAULT_OVERSPEED)         { fprintf(f, "%sOVERSPEED",      first ? "" : ", "); first = 0; }
    if (flags & FAULT_OVERTEMP_HIGH)     { fprintf(f, "%sHIGH_TEMP",      first ? "" : ", "); first = 0; }
}

static void print_faults_inline(FaultFlags flags)
{
    print_faults_inline_to_file(stdout, flags);
}

/* ================================================================
 * JSON Loading Logic
 * ================================================================ */

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

static void load_tests_from_json(void)
{
    FILE *f = fopen("test_cases.json", "r");
    if (!f) {
        printf("[ERROR] Could not open test_cases.json\n");
        return;
    }

    static char buffer[JSON_BUFFER_SIZE];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    buffer[len] = '\0';

    jsmn_parser p;
    jsmntok_t t[MAX_TOKENS];
    jsmn_init(&p);
    int r = jsmn_parse(&p, buffer, len, t, MAX_TOKENS);
    if (r < 0) {
        printf("[ERROR] Failed to parse JSON: %d\n", r);
        return;
    }

    /* Top-level must be an array */
    if (r < 1 || t[0].type != JSMN_ARRAY) {
        printf("[ERROR] JSON root must be an array\n");
        return;
    }

    int i = 1;
    g_num_loaded_tests = 0;

    while (i < r && g_num_loaded_tests < MAX_TESTS) {
        if (t[i].type != JSMN_OBJECT) { i++; continue; }

        TestDefinition *td = &g_tests[g_num_loaded_tests];
        memset(td, 0, sizeof(TestDefinition));
        int obj_size = t[i].size;
        i++;

        for (int j = 0; j < obj_size; j++) {
            if (jsoneq(buffer, &t[i], "name") == 0) {
                int slen = t[i+1].end - t[i+1].start;
                if (slen > 63) slen = 63;
                strncpy(td->name, buffer + t[i+1].start, slen);
                i += 2;
            } else if (jsoneq(buffer, &t[i], "expected_desc") == 0) {
                int slen = t[i+1].end - t[i+1].start;
                if (slen > 127) slen = 127;
                strncpy(td->expected_desc, buffer + t[i+1].start, slen);
                i += 2;
            } else if (jsoneq(buffer, &t[i], "exp_mode") == 0) {
                td->exp_mode = (Mode)atoi(buffer + t[i+1].start);
                i += 2;
            } else if (jsoneq(buffer, &t[i], "exp_state") == 0) {
                td->exp_state = (SystemState)atoi(buffer + t[i+1].start);
                i += 2;
            } else if (jsoneq(buffer, &t[i], "exp_faults") == 0) {
                td->exp_faults = (FaultFlags)atoi(buffer + t[i+1].start);
                i += 2;
            } else if (jsoneq(buffer, &t[i], "cycles") == 0) {
                int array_size = t[i+1].size;
                td->num_cycles = (uint16_t)array_size;
                i += 2; /* Move past 'cycles' key and the array start token */
                for (int k = 0; k < array_size && k < MAX_CYCLES_PER_TEST; k++) {
                    int cycle_obj_size = t[i].size;
                    i++;
                    for (int n = 0; n < cycle_obj_size; n++) {
                        if (jsoneq(buffer, &t[i], "speed") == 0) {
                            td->cycles[k].speed = (int16_t)atoi(buffer + t[i+1].start);
                            i += 2;
                        } else if (jsoneq(buffer, &t[i], "temperature") == 0) {
                            td->cycles[k].temperature = (int16_t)atoi(buffer + t[i+1].start);
                            i += 2;
                        } else if (jsoneq(buffer, &t[i], "gear") == 0) {
                            td->cycles[k].gear = (int8_t)atoi(buffer + t[i+1].start);
                            i += 2;
                        } else if (jsoneq(buffer, &t[i], "mode") == 0) {
                            td->cycles[k].requested_mode = (Mode)atoi(buffer + t[i+1].start);
                            i += 2;
                        } else {
                            i++; /* Unknown key */
                        }
                    }
                }
            } else {
                i++; /* Unknown key */
            }
        }
        g_num_loaded_tests++;
    }
}

/* ================================================================ */

void run_all_test_cases(VehicleStatus *status, FaultStatus *faults)
{
    uint16_t t, c;
    uint64_t tsc0, tsc1;
    uint32_t pass_count = 0U;
    uint32_t fail_count = 0U;

    /* Load from JSON instead of hardcoded array */
    load_tests_from_json();

    if (g_num_loaded_tests == 0) {
        printf("[ERROR] No test cases loaded.\n");
        return;
    }

    FILE *logfile = fopen("log.txt", "w");
    if (logfile == NULL) {
        printf("[ERROR] Could not create log.txt\n");
        return;
    }

    printf("\n========================================================\n");
    printf("  VEHICLE ECU SIMULATOR - DYNAMIC TEST RESULTS\n");
    printf("  (Loaded %u tests from test_cases.json)\n", g_num_loaded_tests);
    printf("========================================================\n");

    fprintf(logfile, "=== VEHICLE ECU TEST LOGS ===\n");
    fprintf(logfile, "Loaded from test_cases.json\n\n");

    for (t = 0U; t < g_num_loaded_tests; t++) {
        uint64_t test_total_cycles = 0U;
        const TestDefinition *test = &g_tests[t];
        const VehicleInput *last_input = &test->cycles[test->num_cycles - 1U];

        fprintf(logfile, "\n>>> TEST %s <<<\n", test->name);
        fprintf(logfile, "    Expected Summary: %s\n", test->expected_desc);

        g_suppress_io = 1;
        init_system(status, faults);
        g_suppress_io = 0;

        for (c = 0U; c < test->num_cycles; c++) {
            VehicleInput input = test->cycles[c];
            CycleTiming timing = {0};

            clear_all_faults(faults);
            g_suppress_io = 1;

            tsc0 = read_tsc(); validate_inputs(&input, status, faults); tsc1 = read_tsc();
            timing.validate_inputs = tsc1 - tsc0;

            tsc0 = read_tsc(); update_mode(status, &input, faults); tsc1 = read_tsc();
            timing.update_mode = tsc1 - tsc0;

            tsc0 = read_tsc(); run_control_checks(&input, status, faults); tsc1 = read_tsc();
            timing.run_control_checks = tsc1 - tsc0;

            tsc0 = read_tsc(); update_fault_status(faults); tsc1 = read_tsc();
            timing.update_fault_status = tsc1 - tsc0;

            tsc0 = read_tsc(); evaluate_system_state(status, faults); tsc1 = read_tsc();
            timing.evaluate_system_state = tsc1 - tsc0;

            g_suppress_io = 0;
            timing.total = timing.validate_inputs + timing.update_mode + 
                           timing.run_control_checks + timing.update_fault_status + 
                           timing.evaluate_system_state;
            test_total_cycles += timing.total;

            log_cycle_summary(&input, status, faults, logfile);
            log_cycle_timing(logfile, c + 1U, &timing);
        }

        int mode_match   = (status->current_mode == test->exp_mode);
        int state_match  = (status->system_state == test->exp_state);
        int faults_match = (faults->active_faults == test->exp_faults);
        int pass = (mode_match && state_match && faults_match);
        const char *result_str = pass ? "PASS" : "FAIL";

        if (pass) pass_count++; else fail_count++;

        fprintf(logfile, "\n  [RESULT] %s\n", result_str);
        if (!pass) {
            fprintf(logfile, "  [DETAILS] Mismatches found:\n");
            if (!mode_match)   fprintf(logfile, "    - Mode mismatch: Exp=%d, Act=%d\n", test->exp_mode, status->current_mode);
            if (!state_match)  fprintf(logfile, "    - State mismatch: Exp=%d, Act=%d\n", test->exp_state, status->system_state);
            if (!faults_match) fprintf(logfile, "    - Faults mismatch: Exp=0x%08X, Act=0x%08X\n", test->exp_faults, faults->active_faults);
        }
        fprintf(logfile, "  [PERF] Test Total: %llu CPU cycles\n", (unsigned long long)test_total_cycles);

        /* Console Summary */
        printf("\n  TEST %s [%s]\n", test->name, result_str);
        printf("  --------------------------------------------------------\n");
        printf("  Input    : Speed=%-3d  Temp=%-3d  Gear=%d  Mode=%s\n",
               last_input->speed, last_input->temperature, last_input->gear, mode_to_name(last_input->requested_mode));
        printf("  Expected : Mode=%-12s  State=%-8s  Faults=",
               mode_to_name(test->exp_mode), state_to_name(test->exp_state));
        print_faults_inline_to_file(stdout, test->exp_faults);
        printf("\n             (%s)\n", test->expected_desc);
        printf("  Actual   : Mode=%-12s  State=%-8s  Faults=",
               mode_to_name(status->current_mode), state_to_name(status->system_state));
        print_faults_inline(faults->active_faults);
        printf("\n             Counters: OS=%u CritT=%u HighT=%u InvGear=%u IllMode=%u\n",
               faults->overspeed_counter, faults->overtemp_critical_counter, 
               faults->overtemp_high_counter, faults->invalid_gear_counter, faults->illegal_mode_counter);
        printf("  CPU Time : %llu cycles\n", (unsigned long long)test_total_cycles);
        printf("  --------------------------------------------------------\n");
    }

    fprintf(logfile, "\n=== SUMMARY: %u Total, %u Passed, %u Failed ===\n", g_num_loaded_tests, pass_count, fail_count);
    fclose(logfile);

    printf("\n========================================================\n");
    printf("  SUMMARY: %u Total, %u Passed, %u Failed\n", g_num_loaded_tests, pass_count, fail_count);
    printf("  Detailed logs + CPU cycle data saved to log.txt\n");
    printf("========================================================\n");
}
