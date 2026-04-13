/**
 * @file ecu_control.c
 * @brief ECU test controller — automated test runner with persistent state and delta inputs.
 * Refactored for cumulative flow and explicit system reset.
 */

#include "types.h"
#include "input.h"
#include "mode.h"
#include "control.h"
#include "fault.h"
#include "state.h"
#include "log.h"
#include "perf.h"
#include "system_io.h"

#define JSMN_STATIC
#include "jsmn.h"

/* No standard headers included here directly - all abstracted via system_io.h */

#define MAX_TESTS           20U
#define MAX_CYCLES_PER_TEST 10U
#define JSON_BUFFER_SIZE    32768U
#define MAX_TOKENS          2048U

typedef struct {
    char           name[64];
    char           expected_desc[128];
    uint16_t       num_cycles;
    VehicleInput   cycles[MAX_CYCLES_PER_TEST];
    int8_t         reset_triggered[MAX_CYCLES_PER_TEST];
    
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
    const char *s;
    switch (m) {
        case MODE_OFF:         s = "OFF";         break;
        case MODE_ACC:         s = "ACC";         break;
        case MODE_IGNITION_ON: s = "IGNITION_ON"; break;
        case MODE_FAULT:       s = "FAULT";       break;
        case MODE_UNKNOWN:
        default:               s = "UNKNOWN";     break;
    }
    return s;
}

static const char* state_to_name(SystemState s)
{
    const char *str;
    switch (s) {
        case STATE_NORMAL:   str = "NORMAL";   break;
        case STATE_DEGRADED: str = "DEGRADED"; break;
        case STATE_SAFE:     str = "SAFE";     break;
        case STATE_UNKNOWN:
        default:             str = "UNKNOWN";  break;
    }
    return str;
}

static void print_faults_inline_to_file(sys_file_t f, FaultFlags flags)
{
    if (flags == 0U) {
        sys_fprintf(f, "NONE");
    } else {
        int first = 1;
        if ((flags & FAULT_OVERTEMP_CRITICAL) != 0U) { sys_fprintf(f, "%sCRIT_OVERHEAT",  (first != 0) ? "" : ", "); first = 0; }
        if ((flags & FAULT_INVALID_GEAR) != 0U)      { sys_fprintf(f, "%sINVALID_GEAR",   (first != 0) ? "" : ", "); first = 0; }
        if ((flags & FAULT_ILLEGAL_MODE) != 0U)      { sys_fprintf(f, "%sILLEGAL_MODE",   (first != 0) ? "" : ", "); first = 0; }
        if ((flags & FAULT_OVERSPEED) != 0U)         { sys_fprintf(f, "%sOVERSPEED",      (first != 0) ? "" : ", "); first = 0; }
        if ((flags & FAULT_OVERTEMP_HIGH) != 0U)     { sys_fprintf(f, "%sHIGH_TEMP",      (first != 0) ? "" : ", "); first = 0; }
    }
}

static void print_faults_inline(FaultFlags flags)
{
    print_faults_inline_to_file(NULL, flags);
}

/* ================================================================
 * JSON Loading Logic
 * ================================================================ */

static int32_t jsoneq(const char *json, const jsmntok_t *tok, const char *s) {
    int32_t res = -1;
    if ((tok->type == JSMN_STRING) && ((int32_t)sys_strlen(s) == (tok->end - tok->start))) {
        if (sys_strncmp(json + tok->start, s, (size_t)(tok->end - tok->start)) == 0) {
            res = 0;
        }
    }
    return res;
}

static void load_tests_from_json(void)
{
    sys_file_t f = sys_fopen("test_cases.json", "r");
    if (f == NULL) {
        sys_log_error("Could not open test_cases.json");
        return;
    }

    static char json_buffer[JSON_BUFFER_SIZE];
    size_t len = sys_fread(json_buffer, 1, sizeof(json_buffer) - 1, f);
    sys_fclose(f);
    json_buffer[len] = '\0';

    jsmn_parser p;
    jsmntok_t t[MAX_TOKENS];
    jsmn_init(&p);
    int r = jsmn_parse(&p, json_buffer, (size_t)len, t, MAX_TOKENS);
    if (r < 0) {
        sys_printf("[ERROR] Failed to parse JSON: %d\n", r);
        return;
    }

    if (r < 1 || t[0].type != JSMN_ARRAY) {
        sys_log_error("JSON root must be an array");
        return;
    }

    int i = 1;
    g_num_loaded_tests = 0;

    while (i < r && g_num_loaded_tests < MAX_TESTS) {
        if (t[i].type != JSMN_OBJECT) { i++; continue; }

        TestDefinition *td = &g_tests[g_num_loaded_tests];
        sys_memset(td, 0, sizeof(TestDefinition));
        
        /* Pre-fill all cycle fields with INPUT_SENTINEL */
        for (uint16_t c_init = 0U; c_init < MAX_CYCLES_PER_TEST; c_init++) {
            td->cycles[c_init].speed = INPUT_SENTINEL;
            td->cycles[c_init].temperature = INPUT_SENTINEL;
            td->cycles[c_init].gear = (int8_t)INPUT_SENTINEL;
            td->cycles[c_init].requested_mode = (Mode)INPUT_SENTINEL;
            td->reset_triggered[c_init] = 0;
        }

        int obj_size = t[i].size;
        i++;

        for (int j = 0; j < obj_size; j++) {
            if (jsoneq(json_buffer, &t[i], "name") == 0) {
                int slen = t[i+1].end - t[i+1].start;
                if (slen > 63) slen = 63;
                sys_strncpy(td->name, json_buffer + t[i+1].start, (size_t)slen);
                i += 2;
            } else if (jsoneq(json_buffer, &t[i], "expected_desc") == 0) {
                int slen = t[i+1].end - t[i+1].start;
                if (slen > 127) slen = 127;
                sys_strncpy(td->expected_desc, json_buffer + t[i+1].start, (size_t)slen);
                i += 2;
            } else if (jsoneq(json_buffer, &t[i], "exp_mode") == 0) {
                td->exp_mode = (Mode)sys_atoi(json_buffer + t[i+1].start);
                i += 2;
            } else if (jsoneq(json_buffer, &t[i], "exp_state") == 0) {
                td->exp_state = (SystemState)sys_atoi(json_buffer + t[i+1].start);
                i += 2;
            } else if (jsoneq(json_buffer, &t[i], "exp_faults") == 0) {
                td->exp_faults = (FaultFlags)sys_atoi(json_buffer + t[i+1].start);
                i += 2;
            } else if (jsoneq(json_buffer, &t[i], "cycles") == 0) {
                int array_size = t[i+1].size;
                td->num_cycles = (uint16_t)array_size;
                i += 2;
                for (int k = 0; k < array_size && k < (int)MAX_CYCLES_PER_TEST; k++) {
                    int cycle_obj_size = t[i].size;
                    i++;
                    for (int n = 0; n < cycle_obj_size; n++) {
                        if (jsoneq(json_buffer, &t[i], "speed") == 0) {
                            td->cycles[k].speed = (int16_t)sys_atoi(json_buffer + t[i+1].start);
                            i += 2;
                        } else if (jsoneq(json_buffer, &t[i], "temperature") == 0) {
                            td->cycles[k].temperature = (int16_t)sys_atoi(json_buffer + t[i+1].start);
                            i += 2;
                        } else if (jsoneq(json_buffer, &t[i], "gear") == 0) {
                            td->cycles[k].gear = (int8_t)sys_atoi(json_buffer + t[i+1].start);
                            i += 2;
                        } else if (jsoneq(json_buffer, &t[i], "mode") == 0) {
                            td->cycles[k].requested_mode = (Mode)sys_atoi(json_buffer + t[i+1].start);
                            i += 2;
                        } else if (jsoneq(json_buffer, &t[i], "reset") == 0) {
                            /* Parse boolean - handle "true" or "1" */
                            if (sys_strncmp(json_buffer + t[i+1].start, "true", 4) == 0 || 
                                sys_strncmp(json_buffer + t[i+1].start, "1", 1) == 0) {
                                td->reset_triggered[k] = 1;
                            }
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
    if ((status != NULL) && (faults != NULL)) {
        uint16_t t, c;
        uint64_t tsc0, tsc1;
        uint32_t pass_count = 0U;
        uint32_t fail_count = 0U;
        VehicleInput active_input;
        sys_memset(&active_input, 0, sizeof(VehicleInput));

        /* Load from JSON */
        load_tests_from_json();

        if (g_num_loaded_tests == 0U) {
            sys_log_error("No test cases loaded.");
            return;
        }

        sys_file_t logfile = sys_fopen("log.txt", "w");
        if (logfile == NULL) {
            sys_log_error("Could not create log.txt");
            return;
        }

        sys_printf("\n========================================================\n");
        sys_printf("  VEHICLE ECU SIMULATOR - PERSISTENT TEST SESSION\n");
        sys_printf("  (Loaded %u cumulative tests from test_cases.json)\n", (unsigned int)g_num_loaded_tests);
        sys_printf("========================================================\n");

        sys_fprintf(logfile, "=== VEHICLE ECU PERSISTENT TEST LOGS ===\n");
        sys_fprintf(logfile, "Cumulative flow enabled. Factory state initialized.\n\n");

        /* Factory Initial State */
        g_suppress_io = 1;
        init_system(status, faults);
        g_suppress_io = 0;

        for (t = 0U; t < g_num_loaded_tests; t++) {
            uint64_t test_total_cycles = 0U;
            const TestDefinition *test = &g_tests[t];

            sys_fprintf(logfile, "\n>>> TEST %s <<<\n", test->name);
            sys_fprintf(logfile, "    Expected Summary: %s\n", test->expected_desc);

            for (c = 0U; c < test->num_cycles; c++) {
                CycleTiming timing = {0};

                /* Check for explicit system reset */
                if (test->reset_triggered[c] != 0) {
                    g_suppress_io = 1;
                    init_system(status, faults);
                    sys_memset(&active_input, 0, sizeof(VehicleInput));
                    g_suppress_io = 0;
                    sys_fprintf(logfile, "  [SYSTEM] Manual Reset Triggered\n");
                }

                /* Apply Delta Update to Active Input */
                const VehicleInput *delta = &test->cycles[c];
                if (delta->speed != INPUT_SENTINEL)            active_input.speed = delta->speed;
                if (delta->temperature != INPUT_SENTINEL)      active_input.temperature = delta->temperature;
                if (delta->gear != (int8_t)INPUT_SENTINEL)     active_input.gear = delta->gear;
                if (delta->requested_mode != (Mode)INPUT_SENTINEL) active_input.requested_mode = delta->requested_mode;

                /* Scheduler Flow per Cycle (Cumulative) */
                clear_all_faults(faults);
                g_suppress_io = 1;

                /* Local copy of active_input to pass to modules (maintaining stability) */
                VehicleInput current_cycle_input = active_input;

                tsc0 = read_tsc(); validate_inputs(&current_cycle_input, status, faults); tsc1 = read_tsc();
                timing.validate_inputs = tsc1 - tsc0;
                /* Update active_input with any corrections from validate_inputs */
                active_input = current_cycle_input;

                tsc0 = read_tsc(); update_mode(status, &active_input, faults); tsc1 = read_tsc();
                timing.update_mode = tsc1 - tsc0;

                tsc0 = read_tsc(); run_control_checks(&active_input, status, faults); tsc1 = read_tsc();
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

                log_cycle_summary(&active_input, status, faults, logfile);
                log_cycle_timing(logfile, c + 1U, &timing);
            }

            int mode_match   = (status->current_mode == test->exp_mode);
            int state_match  = (status->system_state == test->exp_state);
            int faults_match = (faults->active_faults == test->exp_faults);
            int pass = (mode_match && state_match && faults_match);
            const char *result_str = (pass != 0) ? "PASS" : "FAIL";

            if (pass != 0) { pass_count++; } else { fail_count++; }

            sys_fprintf(logfile, "\n  [RESULT] %s\n", result_str);
            if (pass == 0) {
                sys_fprintf(logfile, "  [DETAILS] Mismatches against cumulative baseline:\n");
                if (mode_match == 0)   sys_fprintf(logfile, "    - Mode mismatch: Exp=%d, Act=%d\n", (int)test->exp_mode, (int)status->current_mode);
                if (state_match == 0)  sys_fprintf(logfile, "    - State mismatch: Exp=%d, Act=%d\n", (int)test->exp_state, (int)status->system_state);
                if (faults_match == 0) sys_fprintf(logfile, "    - Faults mismatch: Exp=0x%08X, Act=0x%08X\n", (unsigned int)test->exp_faults, (unsigned int)faults->active_faults);
            }
            sys_fprintf(logfile, "  [PERF] Test Total: %llu CPU cycles\n", (unsigned long long)test_total_cycles);

            /* Console Summary */
            sys_printf("\n  TEST %s [%s]\n", test->name, result_str);
            sys_printf("  --------------------------------------------------------\n");
            sys_printf("  Active   : Speed=%-3d  Temp=%-3d  Gear=%d  Mode=%s\n",
                   (int)active_input.speed, (int)active_input.temperature, (int)active_input.gear, mode_to_name(active_input.requested_mode));
            sys_printf("  Expected : Mode=%-12s  State=%-8s  Faults=",
                   mode_to_name(test->exp_mode), state_to_name(test->exp_state));
            print_faults_inline_to_file(NULL, test->exp_faults);
            sys_printf("\n             (%s)\n", test->expected_desc);
            sys_printf("  Current  : Mode=%-12s  State=%-8s  Faults=",
                   mode_to_name(status->current_mode), state_to_name(status->system_state));
            print_faults_inline(faults->active_faults);
            sys_printf("\n             Counters: OS=%u CritT=%u HighT=%u InvGear=%u IllMode=%u\n",
                   (unsigned int)faults->overspeed_counter, (unsigned int)faults->overtemp_critical_counter, 
                   (unsigned int)faults->overtemp_high_counter, (unsigned int)faults->invalid_gear_counter, (unsigned int)faults->illegal_mode_counter);
            sys_printf("  CPU Time : %llu cycles\n", (unsigned long long)test_total_cycles);
            sys_printf("  --------------------------------------------------------\n");
        }

        sys_fprintf(logfile, "\n=== SUMMARY: %u Total, %u Passed, %u Failed ===\n", (unsigned int)g_num_loaded_tests, (unsigned int)pass_count, (unsigned int)fail_count);
        sys_fclose(logfile);

        sys_printf("\n========================================================\n");
        sys_printf("  SUMMARY: %u Total, %u Passed, %u Failed\n", (unsigned int)g_num_loaded_tests, (unsigned int)pass_count, (unsigned int)fail_count);
        sys_printf("  Detailed logs + CPU cycle data saved to log.txt\n");
        sys_printf("========================================================\n");
    }
}
