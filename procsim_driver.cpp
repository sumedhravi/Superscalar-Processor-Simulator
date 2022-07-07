// You do not need to modify this file!

#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "procsim.hpp"

static size_t n_insts;
static inst_t *insts;
static uint64_t fetch_inst_idx;

// Print error usage
static void print_err_usage(const char *err) {
    fprintf(stderr, "%s\n", err);
    fprintf(stderr, "./procsim -I <trace file> [Options]\n");
    fprintf(stderr, "-R <number of ROB entries>\n");
    fprintf(stderr, "-S <number of SchedQ entries per FU>\n");
    fprintf(stderr, "-C <dcache C, from (C,B,S)>\n");
    fprintf(stderr, "-A <number of ALU FUs>\n");
    fprintf(stderr, "-M <number of MUL FUs>\n");
    fprintf(stderr, "-L <number of load/store FUs>\n");
    fprintf(stderr, "-F <fetch width>\n");
    fprintf(stderr, "-W <retire width>\n");
    fprintf(stderr, "-D disables interupts\n");
    fprintf(stderr, "-H prints this message\n");

    exit(EXIT_FAILURE);
}

// Function to print the run configuration
static void print_sim_config(procsim_conf_t *sim_conf) {
    printf("SIMULATION CONFIGURATION\n");
    printf("ROB entries:  %lu\n", sim_conf->num_rob_entries);
    printf("Num. SchedQ entries per FU: %lu\n", sim_conf->num_schedq_entries_per_fu);
    printf("Data cache C: %lu\n", sim_conf->dcache_c);
    printf("Data cache B: %d\n", DCACHE_B);
    printf("Data cache S: %d\n", DCACHE_S);
    printf("Num. ALU FUs: %lu\n", sim_conf->num_alu_fus);
    printf("Num. MUL FUs: %lu\n", sim_conf->num_mul_fus);
    printf("Num. LSU FUs: %lu\n", sim_conf->num_lsu_fus);
    printf("Fetch width:  %lu\n", sim_conf->fetch_width);
    printf("Retire width: %lu\n", sim_conf->retire_width);
    printf("Interrupts:   %s\n", sim_conf->interrupts_enabled? "enabled"
                                                             : "disabled");
}

// Function to print the simulation output
static void print_sim_output(procsim_stats_t *sim_stats) {
    printf("\nSIMULATION OUTPUT\n");
    printf("Cycles:               %" PRIu64 "\n", sim_stats->cycles);
    printf("Interrupts:           %" PRIu64 "\n", sim_stats->interrupts);
    printf("Trace instructions:   %" PRIu64 "\n", sim_stats->instructions_in_trace);
    printf("Fetched instructions: %" PRIu64 "\n", sim_stats->instructions_fetched);
    printf("Retired instructions: %" PRIu64 "\n", sim_stats->instructions_retired);
    printf("Writes to arch. RF:   %" PRIu64 "\n", sim_stats->arf_writes);
    printf("DCache reads:            %" PRIu64 "\n", sim_stats->dcache_reads);
    printf("DCache read misses:      %" PRIu64 "\n", sim_stats->dcache_read_misses);
    printf("DCache read miss ratio:  %.3f\n", sim_stats->dcache_read_miss_ratio);
    printf("DCache read AAT:         %.3f\n", sim_stats->dcache_read_aat);
    printf("Cycles with no fires:    %" PRIu64 "\n", sim_stats->no_fire_cycles);
    printf("Stall cycles due to ROB: %" PRIu64 "\n", sim_stats->rob_stall_cycles);
    printf("Max DispQ usage:      %" PRIu64 "\n", sim_stats->dispq_max_usage);
    printf("Average DispQ usage:  %.3f\n", sim_stats->dispq_avg_usage);
    printf("Max SchedQ usage:     %" PRIu64 "\n", sim_stats->schedq_max_usage);
    printf("Average SchedQ usage: %.3f\n", sim_stats->schedq_avg_usage);
    printf("Max ROB usage:        %" PRIu64 "\n", sim_stats->rob_max_usage);
    printf("Average ROB usage:    %.3f\n", sim_stats->rob_avg_usage);
    printf("IPC:                  %.3f\n", sim_stats->ipc);
}

static inst_t *read_entire_trace(FILE *trace, size_t *size_insts_out, procsim_conf_t *sim_conf) {
    static const size_t interrupt_interval = 31873;
    size_t size_insts = 0;
    size_t cap_insts = 0;
    inst_t *insts_arr = NULL;

    while (!feof(trace)) {
        if (size_insts == cap_insts) {
            size_t new_cap_insts = 2 * (cap_insts + 1);
            // redundant c++ cast #1
            inst_t *new_insts_arr = (inst_t *)realloc(insts_arr, new_cap_insts * sizeof *insts_arr);
            if (!new_insts_arr) {
                perror("realloc");
                goto error;
            }
            cap_insts = new_cap_insts;
            insts_arr = new_insts_arr;
        }

        inst_t *inst = insts_arr + size_insts;
        int ret = fscanf(trace, "%" SCNx64 " %d %" SCNd8 " %" SCNd8 " %" SCNd8 " %" SCNx64 "\n", &inst->pc, (int *)&inst->opcode, &inst->dest, &inst->src1, &inst->src2, &inst->load_store_addr);

        if (ret == 6) {
            inst->interrupt = sim_conf->interrupts_enabled
                              && size_insts > 0
                              && !(size_insts % interrupt_interval);
            size_insts++;
        } else {
            if (ferror(trace)) {
                perror("fscanf");
            } else {
                fprintf(stderr, "could not parse line in trace (only %d input items matched). is it corrupt?\n", ret);
            }
            goto error;
        }
    }

    *size_insts_out = size_insts;
    return insts_arr;

    error:
    free(insts_arr);
    *size_insts_out = 0;
    return NULL;
}

const inst_t *procsim_driver_read_inst(void) {
    if (fetch_inst_idx >= n_insts) {
        return NULL;
    } else {
        return &insts[fetch_inst_idx++];
    }
}

int main(int argc, char *const argv[])
{
    FILE *trace = NULL;

    procsim_stats_t sim_stats;
    memset(&sim_stats, 0, sizeof sim_stats);

    procsim_conf_t sim_conf;
    memset(&sim_conf, 0, sizeof sim_conf);
    // Default configuration
    sim_conf.num_rob_entries = 32;
    sim_conf.num_schedq_entries_per_fu = 2;
    sim_conf.dcache_c = 13;
    sim_conf.num_alu_fus = 3;
    sim_conf.num_mul_fus = 2;
    sim_conf.num_lsu_fus = 2;
    sim_conf.fetch_width = 4;
    sim_conf.retire_width = 8;
    sim_conf.interrupts_enabled = true;

    int opt;
    while (-1 != (opt = getopt(argc, argv, "i:I:r:R:s:S:c:C:a:A:m:M:l:L:f:F:w:W:dDhH"))) {
        switch (opt) {
            case 'i':
            case 'I':
                trace = fopen(optarg, "r");
                if (trace == NULL) {
                    perror("fopen");
                    print_err_usage("Could not open the input trace file");
                }
                break;

            case 'r':
            case 'R':
                sim_conf.num_rob_entries = atoi(optarg);
                break;

            case 's':
            case 'S':
                sim_conf.num_schedq_entries_per_fu = atoi(optarg);
                break;

            case 'c':
            case 'C':
                sim_conf.dcache_c = atoi(optarg);
                break;

            case 'a':
            case 'A':
                sim_conf.num_alu_fus = atoi(optarg);
                break;

            case 'm':
            case 'M':
                sim_conf.num_mul_fus = atoi(optarg);
                break;

            case 'l':
            case 'L':
                sim_conf.num_lsu_fus = atoi(optarg);
                break;

            case 'f':
            case 'F':
                sim_conf.fetch_width = atoi(optarg);
                break;

            case 'w':
            case 'W':
                sim_conf.retire_width = atoi(optarg);
                break;

            case 'd':
            case 'D':
                sim_conf.interrupts_enabled = false;
                break;

            case 'h':
            case 'H':
                print_err_usage("");
                break;

            default:
                print_err_usage("Invalid argument to program");
                break;
        }
    }

    if (!trace) {
        print_err_usage("No trace file provided!");
    }

    insts = read_entire_trace(trace, &n_insts, &sim_conf);
    fclose(trace);
    if (!insts) {
        return 1;
    }

    print_sim_config(&sim_conf);
    // Initialize the processor
    procsim_init(&sim_conf, &sim_stats);
    printf("SETUP COMPLETE - STARTING SIMULATION\n");

    // We made this number up, but it should never take this many cycles to
    // retire something
    static const uint64_t max_cycles_since_last_retire = 64;
    uint64_t cycles_since_last_retire = 0;

    uint64_t retired_inst_idx = 0;
    fetch_inst_idx = 0;
    while (retired_inst_idx < n_insts) {
        bool retired_interrupt = false;
        uint64_t retired_this_cycle = procsim_do_cycle(&sim_stats, &retired_interrupt);
        retired_inst_idx += retired_this_cycle;

        // Check for deadlocks (e.g., an empty submission)
        if (retired_this_cycle) {
            cycles_since_last_retire = 0;
        } else {
            cycles_since_last_retire++;
        }
        if (cycles_since_last_retire == max_cycles_since_last_retire) {
            printf("\nIt has been %" PRIu64 " cycles since the last retirement."
                   " Does the simulator have a deadlock?\n",
                   max_cycles_since_last_retire);
            return 1;
        }

        if (retired_interrupt) {
            // Start refilling the dispatch queue where we left off to service
            // the interrupt
            fetch_inst_idx = retired_inst_idx;
        }
    }

    sim_stats.instructions_in_trace = n_insts;

    // Free memory and generate final statistics
    procsim_finish(&sim_stats);
    free(insts);

    print_sim_output(&sim_stats);

    return 0;
}
