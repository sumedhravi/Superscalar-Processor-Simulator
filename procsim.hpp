#ifndef PROCSIM_H
#define PROCSIM_H

#include <inttypes.h>

// Number of architectural registers / GPRs
#define NUM_REGS 32
#define L2_LATENCY_CYCLES 9
#define DCACHE_S 0
#define DCACHE_B 6
#define ALU_STAGES 1
#define MUL_STAGES 3
#define MEM_STAGES 1

#ifdef DEBUG
// Beautiful preprocessor hack to convert some syntax (e.g., a constant int) to
// a string. Only used for printing debug outputs
#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#endif

typedef enum {
    OPCODE_ADD = 2,
    OPCODE_MUL,
    OPCODE_LOAD,
    OPCODE_STORE,
    OPCODE_BRANCH,
} opcode_t;

typedef struct {
    uint64_t pc;
    opcode_t opcode;
    int8_t dest;
    int8_t src1;
    int8_t src2;
    uint64_t load_store_addr;
    // A value of true indicates that this instruction is an interrupt. The
    // driver sets this flag for you. This bit should be passed through the
    // pipeline all the way to the State Update stage.
    bool interrupt;
} inst_t;

// This config struct is populated by the driver for you
typedef struct {
    size_t num_rob_entries;
    size_t num_schedq_entries_per_fu;
    size_t dcache_c;
    size_t num_alu_fus;
    size_t num_mul_fus;
    size_t num_lsu_fus;
    size_t fetch_width;
    size_t retire_width;

    // You can ignore this; it's used only by the driver
    bool interrupts_enabled;
} procsim_conf_t;

typedef struct {
    // You need to update these! You can assume all are initialized to 0.
    uint64_t cycles;
    // Incremented when we retire an interrupting instruction
    uint64_t interrupts;
    uint64_t instructions_fetched;
    uint64_t instructions_retired;
    // Incremented when we write to the architectural register file (that is,
    // when we retire an instruction with dest != -1)
    uint64_t arf_writes;
    uint64_t dcache_reads;
    uint64_t dcache_read_misses;
    // Incremented for each cycle in which we fired no instructions
    uint64_t no_fire_cycles;
    // Incremented for each cycle in which we stop dispatching only because of
    // insufficient ROB space
    uint64_t rob_stall_cycles;
    // Maximum valid DispQ entries at the END of procsim_do_cycle()
    uint64_t dispq_max_usage;
    // Maximum valid SchedQ entries at the END of procsim_do_cycle()
    uint64_t schedq_max_usage;
    // Maximum valid ROB entries at the END of procsim_do_cycle()
    uint64_t rob_max_usage;
    // Average valid DispQ entries at the END of procsim_do_cycle()
    double dispq_avg_usage;
    // Average valid SchedQ entries at the END of procsim_do_cycle()
    double schedq_avg_usage;
    // Average valid ROB entries at the END of procsim_do_cycle()
    double rob_avg_usage;
    double dcache_read_miss_ratio;
    double dcache_read_aat;
    double ipc;

    // The driver populates the stat below for you
    uint64_t instructions_in_trace;
} procsim_stats_t;

// We have implemented this function for you in the driver. By calling it, you
// are effectively reading from an icache with 100% hit rate, where branch
// prediction is 100% correct and handled for you.
extern const inst_t *procsim_driver_read_inst(void);

// There is more information on these functions in procsim.cpp
extern void procsim_init(const procsim_conf_t *sim_conf,
                         procsim_stats_t *stats);
extern uint64_t procsim_do_cycle(procsim_stats_t *stats,
                                 bool *retired_interrupt_out);
extern void procsim_finish(procsim_stats_t *stats);

#endif
