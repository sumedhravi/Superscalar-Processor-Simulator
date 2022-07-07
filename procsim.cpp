#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "procsim.hpp"

#include <vector>
#include <list>
#include <unordered_map>
#include <math.h>
#include <iterator>

#include <iostream>
#include <string>
//
// TODO: Define any useful data structures and functions here
//

// Structure to store cache tag data
struct address_block
{
    uint64_t tag;

    address_block(uint64_t tag)
    {
        this->tag = tag;
    }
};

// Class to represent a direct mapped data cache
class data_cache
{
    private:
        std::vector<address_block> blocks;
    public:
        uint64_t cache_size;
        uint64_t block_size;
        data_cache(uint64_t c, uint64_t block_size = 6);
        bool access(uint64_t element);
        void insert(uint64_t element);
        uint64_t generate_tag(uint64_t address);
        uint64_t generate_index(uint64_t address);
};

// Init
data_cache::data_cache(uint64_t c, uint64_t block_size)
{
    cache_size = c;
    this->block_size = block_size;
    blocks = std::vector<address_block>(int(pow(2, cache_size-block_size)), address_block(0));
}

// Get Tag value from address
uint64_t data_cache:: generate_tag(uint64_t address)
{
    uint64_t tag = address >> cache_size; // tag = shift by c bits to get tag 
    return tag;
}

// Get index value from address
uint64_t data_cache:: generate_index(uint64_t address)
{
    uint64_t tag = address >> cache_size; // shift by (c) bits to get tag
    uint64_t index = address - (tag << cache_size);
    index = index >> (block_size); // b
    return index;
}

// Access an address block in the cache
bool data_cache::access(uint64_t element)
{
    uint64_t tag = generate_tag(element);
    int index = generate_index(element);
    auto block = &blocks[index];
    if (block->tag == tag)
    {
        return true;
    }
    else
        return false;
}

// Add a block to the cache
void data_cache::insert(uint64_t element)
{
    uint64_t tag = generate_tag(element);
    int index = generate_index(element);
    auto block = &blocks[index];
    block->tag = tag;
}

// Structure to represent a reservation station 
struct reservation_station
{
    int8_t dest;
    int8_t src1;
    int8_t src2;
    uint64_t dest_tag;
    uint64_t src1_tag;
    uint64_t src2_tag;
    bool src1_ready;
    bool src2_ready;
    opcode_t opcode;
    uint64_t load_store_addr;
    bool is_interrupt;
    bool is_fired;

    reservation_station(const inst_t inst, const uint64_t dest_tag, const uint64_t src1_tag, const uint64_t src2_tag, 
                        const bool src1_ready, const bool src2_ready, const bool fired = false)
    {
        this->dest            = inst.dest;
        this->src1            = inst.src1;
        this->src2            = inst.src2;
        this->dest_tag        = dest_tag;
        this->src1_tag        = src1_tag;
        this->src2_tag        = src2_tag;
        this->src1_ready      = src1_ready;
        this->src2_ready      = src2_ready;
        this->opcode          = inst.opcode;
        this->load_store_addr = inst.load_store_addr;
        this->is_interrupt    = inst.interrupt;
        is_fired = fired;
    }
};

// Class to represent a scheduling queue consisting of reservation stations
class scheduling_queue
{
    public:
    std::list<reservation_station> reservation_stations;
    std::unordered_map<uint64_t, std::list<reservation_station>::iterator> hashmap;
    uint64_t capacity;

    scheduling_queue(uint64_t capacity)
    {
        this->capacity = capacity;
    }

    uint64_t num_of_free_slots()
    {
        return capacity - hashmap.size();
    }

    void add(inst_t inst, uint64_t dest_tag, uint64_t src1_tag, uint64_t src2_tag, bool src1_ready, bool src2_ready)
    {
        #ifdef DEBUG
        if (inst.src1 == -1 and inst.src2 == -1)
            printf(". Assigning tag=%" PRIu64 " and setting src1_ready=1 and src2_ready=1\n", dest_tag);
        else if (inst.src1 != -1 and inst.src2 != -1)
            printf(". Assigning tag=%" PRIu64 " and setting src1_tag=%" PRIu64 ", src1_ready=%d, src2_tag=%" PRIu64 ", and src2_ready=%d\n", dest_tag, src1_tag, src1_ready, src2_tag, src2_ready);
        else if (inst.src1 == -1)
            printf(". Assigning tag=%" PRIu64 " and setting src1_ready=%d, src2_tag=%" PRIu64 ", and src2_ready=%d\n", dest_tag, src1_ready, src2_tag, src2_ready);
        else
            printf(". Assigning tag=%" PRIu64 " and setting src1_tag=%" PRIu64 ", src1_ready=%d, and src2_ready=%d\n", dest_tag, src1_tag, src1_ready, src2_ready);
        #endif
        reservation_stations.push_back(reservation_station(inst, dest_tag, src1_tag, src2_tag, src1_ready, src2_ready));
        hashmap[dest_tag] = --(reservation_stations.end());
    }

    void complete(uint64_t tag)
    {
        reservation_stations.erase(hashmap[tag]);
        hashmap.erase(tag);
    }

    void clear()
    {
        reservation_stations.clear();
        hashmap.clear();
    }
};

// Class to represent a register
class register_file
{
    public:
    std::vector<uint64_t> tags;
    std::vector<bool> ready_bits;

    register_file()
    {
        reset(0);
    }

    void reset(uint64_t inital_tag)
    {
        tags = std::vector<uint64_t>(NUM_REGS, 0);
        for (uint64_t i=inital_tag; i<(inital_tag + NUM_REGS); i++)
        {
            tags[i-inital_tag] = i;
        }
        ready_bits = std::vector<bool>(NUM_REGS, true);
    }
};

// Helper structure to represent an instruction that can be stored in a reservation station
struct execution_instruction
{
    uint64_t stage = 0;
    int8_t dest;
    uint64_t dest_tag;
    uint64_t memory_addr;
    opcode_t opcode;
    bool is_interrupt;
    bool is_completed;

    execution_instruction(int8_t dest_reg, uint64_t tag, uint64_t load_store_addr, opcode_t op, bool interrupt, bool completed)
    {
        dest = dest_reg;
        dest_tag = tag;
        memory_addr = load_store_addr;
        opcode = op;
        is_interrupt = interrupt;
        is_completed = completed;
    }
};

// Structure to represent an execution functional unit
struct functional_unit
{
    bool is_pipelined;
    uint64_t num_cycles;
    std::list<execution_instruction> stages;

    functional_unit(bool pipelined, uint64_t execution_cycles)
    {
        is_pipelined = pipelined;
        num_cycles = execution_cycles;
        // stages.resize(num_cycles);
    }

    void execute_instruction(int8_t dest_reg, uint64_t tag, opcode_t op, bool interrupt, uint64_t load_store_addr = 0)
    {
        stages.push_front(execution_instruction(dest_reg, tag, load_store_addr, op, interrupt, false));
    }

    bool update_state()
    {
        if (stages.size() > 0)
        {
            for (auto it=stages.begin(); it != stages.end(); it++)
            {
                it->stage += 1;
            }
            if (stages.back().stage == num_cycles)
            {
                return true;
            }
        }
        return false;
    }

    // Call only if update_state returns true, i.e that is instruction execution is complete.
    execution_instruction complete_execution()
    {
        execution_instruction completed_instruction = stages.back();

        stages.pop_back();
        return completed_instruction;
    }

    bool is_ready()
    {
        if (!is_pipelined)
            return stages.empty();
        return stages.empty() or (stages.front().stage != 0);
    }
};

data_cache* d_cache;
uint64_t fetch_rate;
std::list<inst_t> dispatch_queue;
uint64_t A;
uint64_t L;
uint64_t M;
uint64_t S;
uint64_t W;
scheduling_queue* schedule_queue;
register_file messy_regs;
std::vector<functional_unit> ALU;
std::vector<functional_unit> MUL;
std::vector<functional_unit> LSU;
uint64_t current_tag = 32;
std::list<execution_instruction> rob;
std::unordered_map<uint64_t, uint64_t> load_store_indices;
uint64_t max_rob_entries;

// The helper functions in this #ifdef are optional and included here for your
// convenience so you can spend more time writing your simulator logic and less
// time trying to match debug trace formatting! (If you choose to use them)
#ifdef DEBUG
static void print_operand(int8_t rx) {
    if (rx < 0) {
        printf("(none)");
    } else {
        printf("R%" PRId8, rx);
    }
}

// Useful in the fetch and dispatch stages
static void print_instruction(const inst_t *inst) {
    static const char *opcode_names[] = {NULL, NULL, "ADD", "MUL", "LOAD", "STORE", "BRANCH"};

    printf("opcode=%s, dest=", opcode_names[inst->opcode]);
    print_operand(inst->dest);
    printf(", src1=");
    print_operand(inst->src1);
    printf(", src2=");
    print_operand(inst->src2);
}

static void print_messy_rf(void) {
    for (uint64_t regno = 0; regno < NUM_REGS; regno++) {
        if (regno == 0) {
            printf("    R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, messy_regs.tags[regno], int(messy_regs.ready_bits[regno])); // TODO: fix me
        } else if (!(regno & 0x3)) {
            printf("\n    R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, messy_regs.tags[regno], int(messy_regs.ready_bits[regno])); // TODO: fix me
        } else {
            printf(", R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, messy_regs.tags[regno], int(messy_regs.ready_bits[regno])); // TODO: fix me
        }
    }
    printf("\n");
}

static void print_schedq(void) {
    size_t printed_idx = 0;
    for (auto it = schedule_queue->reservation_stations.begin(); it != schedule_queue->reservation_stations.end(); it++) { // TODO: fix me
        if (printed_idx == 0) {
            printf("    {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", it->is_fired, it->dest_tag,it->src1_tag, it->src1_ready,it->src2_tag, it->src2_ready); // TODO: fix me
        } else if (!(printed_idx & 0x1)) {
            printf("\n    {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", it->is_fired, it->dest_tag,it->src1_tag, it->src1_ready,it->src2_tag, it->src2_ready); // TODO: fix me
        } else {
            printf(", {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", it->is_fired, it->dest_tag,it->src1_tag, it->src1_ready,it->src2_tag, it->src2_ready); // TODO: fix me
        }

        printed_idx++;
    }
    if (!printed_idx) {
        printf("    (scheduling queue empty)");
    }
    printf("\n");
}

static void print_rob(void) {
    size_t printed_idx = 0;
    printf("    Next tag to retire (head of ROB): %" PRIu64 "\n", rob.size() > 0 ? rob.front().dest_tag : 32); // TODO: fix me
    for (auto it = rob.begin(); it != rob.end(); it++) {
        if (!it->is_completed)
            continue;
        if (printed_idx == 0) {
            printf("    { tag: %" PRIu64 ", interrupt: %d }", it->dest_tag, it->is_interrupt); // TODO: fix me
        } else if (!(printed_idx & 0x3)) {
            printf("\n    { tag: %" PRIu64 ", interrupt: %d }", it->dest_tag, it->is_interrupt); // TODO: fix me
        } else {
            printf(", { tag: %" PRIu64 " interrupt: %d }", it->dest_tag, it->is_interrupt); // TODO: fix me
        }

        printed_idx++;
    }
    if (!printed_idx) {
        printf("    (ROB empty)");
    }
    printf("\n");
}
#endif

// Helper method to update reservation stations and mess regs corresponding to the tag
void update_rs_and_messy_regs(execution_instruction completed_instruction)
{
    for (auto rs = schedule_queue->reservation_stations.begin(); rs != schedule_queue->reservation_stations.end(); rs++)
    {
        rs->src1_ready = (rs->src1_ready or rs->src1_tag == completed_instruction.dest_tag);
        rs->src2_ready = (rs->src2_ready or rs->src2_tag == completed_instruction.dest_tag);
    }
    // Update messy registers 
    if (messy_regs.tags[completed_instruction.dest] == completed_instruction.dest_tag)
    {
        messy_regs.ready_bits[completed_instruction.dest] = true;
    }
}

uint64_t get_rob_usage()
{
    uint64_t usage_size = 0;
    for (auto it = rob.begin(); it != rob.end(); it++)
    {
        if (it->is_completed)
            usage_size += 1;
    }

    return usage_size;    
}

// Optional helper function which resets all state on an interrupt, including
// the dispatch queue, scheduling queue, the ROB, etc. Note the dcache should
// NOT be flushed here! The messy register file needs to have all register set
// to ready (and in a real system, we would copy over the values from the
// architectural register file; however, this is a simulation with no values to
// copy!)
static void flush_pipeline(void) {
    // TODO: fill me in
    rob.clear();
    for (uint64_t i=0; i<A; i++)
    {
        ALU[i].stages.clear();
    }
    for (uint64_t i=0; i<M; i++)
    {
        MUL[i].stages.clear();
    }
    for (uint64_t i=0; i<L; i++)
    {
        LSU[i].stages.clear();
    }
    schedule_queue->clear();
    messy_regs.reset(current_tag);
    dispatch_queue.clear();
    load_store_indices.clear();
    current_tag += 32;
}

// Optional helper function which pops instructions from the head of the ROB
// with a maximum of retire_width instructions retired. (In a real system, the
// destination register value from the ROB would be written to the
// architectural register file, but we have no register values in this
// simulation.) This function returns the number of instructions retired.
// Immediately after retiring an interrupt, this function will set
// *retired_interrupt_out = true and return immediately. Note that in this
// case, the interrupt must be counted as one of the retired instructions.
static uint64_t stage_state_update(procsim_stats_t *stats,
                                   bool *retired_interrupt_out) {
    // TODO: fill me in
    *retired_interrupt_out = false;
    load_store_indices.clear();
    uint64_t retired_inst = 0; 
    
    auto it = rob.begin();
    while (it != rob.end() and retired_inst < W)
    {
        if (it->is_completed)
        {
            retired_inst += 1;

            if (it->opcode == OPCODE_LOAD)
            {
                load_store_indices[d_cache->generate_index(it->memory_addr)] = 0;
            } 
            
            if (it->dest != -1)
            {
                #ifdef DEBUG
                printf("Retiring instruction with tag %" PRIu64 ". Writing R%" PRId8 " to architectural register file\n", it->dest_tag, it->dest);
                #endif
                stats->arf_writes += 1; 
            }
            else 
            {
                #ifdef DEBUG
                printf("Retiring instruction with tag %" PRIu64 " (Not writing to architectural register file because no dest)\n", it->dest_tag);
                #endif
            }
            *retired_interrupt_out = it->is_interrupt;
            rob.erase(it++);
            if (*retired_interrupt_out)
                break;
        }
        else
            break;
    }

    stats->instructions_retired += retired_inst;
    return retired_inst;
}

// Optional helper function which is responsible for moving instructions
// through pipelined Function Units and then when instructions complete (that
// is, when instructions are in the final pipeline stage of an FU and aren't
// stalled there), setting the ready bits in the messy register file and
// scheduling queue. This function should remove an instruction from the
// scheduling queue when it has completed.
static void stage_exec(procsim_stats_t *stats) {
    // TODO: fill me in
    for (uint64_t i=0; i<A; i++)
    {
        bool complete = ALU[i].update_state();
        if (complete)
        {
            // Update FU
            execution_instruction instruction = ALU[i].complete_execution();
            #ifdef DEBUG
            if (instruction.dest != -1)
                printf("Instruction with tag %" PRIu64 " and dest reg=R%" PRId8 " completing\n", instruction.dest_tag, instruction.dest);
            else
                printf("Instruction with tag %" PRIu64 " and dest reg=(none) completing\n", instruction.dest_tag);
            #endif
            // Remove Instruction from scheduling queue
            schedule_queue->complete(instruction.dest_tag);
            #ifdef DEBUG
            printf("Removing instruction with tag %" PRIu64 " from the scheduling queue\n", instruction.dest_tag);
            printf("Inserting instruction with tag %" PRIu64 " into the ROB\n", instruction.dest_tag);
            #endif
            // Update ROB
            for (auto rob_entry=rob.begin(); rob_entry!=rob.end(); rob_entry++)
            {
                if (rob_entry->dest_tag == instruction.dest_tag)
                    rob_entry->is_completed = true;
            }
            update_rs_and_messy_regs(instruction);
        }
    }

    for (uint64_t i=0; i<M; i++)
    {
        bool complete = MUL[i].update_state();
        if (complete)
        {
            execution_instruction instruction = MUL[i].complete_execution();
            #ifdef DEBUG
            printf("Instruction with tag %" PRIu64 " and dest reg=R%" PRId8 " completing\n", instruction.dest_tag, instruction.dest);
            #endif
            // Update scheduling queue
            schedule_queue->complete(instruction.dest_tag);
            #ifdef DEBUG
            printf("Removing instruction with tag %" PRIu64 " from the scheduling queue\n", instruction.dest_tag);
            printf("Inserting instruction with tag %" PRIu64 " into the ROB\n", instruction.dest_tag);
            #endif
            // Update ROB
            for (auto rob_entry=rob.begin(); rob_entry!=rob.end(); rob_entry++)
            {
                if (rob_entry->dest_tag == instruction.dest_tag)
                    rob_entry->is_completed = true;
            }
            update_rs_and_messy_regs(instruction);
        }
    }

    for (uint64_t i=0; i<L; i++)
    {
        bool possible_completion = LSU[i].update_state();
         
        if (possible_completion)
        {
            if (LSU[i].stages.front().opcode == OPCODE_LOAD)
            {
                if (LSU[i].num_cycles == 1 and d_cache->access(LSU[i].stages.front().memory_addr) == false)
                {
                    // Cache miss
                    #ifdef DEBUG
                    printf("dcache miss for instruction with tag %" PRIu64 " (dcache index: %" PRIx64 ", dcache tag: %" PRIx64 "), stalling for %d cycles\n", LSU[i].stages.front().dest_tag, d_cache->generate_index(LSU[i].stages.front().memory_addr), d_cache->generate_index(LSU[i].stages.front().memory_addr), L2_LATENCY_CYCLES);
                    #endif
                    LSU[i].num_cycles = 10;
                    stats->dcache_reads += 1;
                    stats->dcache_read_misses += 1;
                }
                else if (LSU[i].num_cycles == 1)
                {
                    // Cache hit
                    stats->dcache_reads += 1;
                    #ifdef DEBUG
                    printf("dcache hit for instruction with tag %" PRIu64 " (dcache index: %" PRIx64 ", dcache tag: %" PRIx64 ")\n", LSU[i].stages.front().dest_tag, d_cache->generate_index(LSU[i].stages.front().memory_addr), d_cache->generate_index(LSU[i].stages.front().memory_addr));
                    #endif
                    execution_instruction completed_instruction = LSU[i].complete_execution();
                    #ifdef DEBUG
                    if (completed_instruction.dest != -1)
                    {
                        printf("Instruction with tag %" PRIu64 " and dest reg=R%" PRId8 " completing\n", completed_instruction.dest_tag, completed_instruction.dest);
                    }
                    else
                    {
                        printf("Instruction with tag %" PRIu64 " and dest reg=(none) completing\n", completed_instruction.dest_tag);
                    }
                    #endif
                    #ifdef DEBUG
                    printf("Removing instruction with tag %" PRIu64 " from the scheduling queue\n", completed_instruction.dest_tag);
                    printf("Inserting instruction with tag %" PRIu64 " into the ROB\n", completed_instruction.dest_tag);
                    #endif
                    schedule_queue->complete(completed_instruction.dest_tag);
                    // Update ROB
                    for (auto rob_entry=rob.begin(); rob_entry!=rob.end(); rob_entry++)
                    {
                        if (rob_entry->dest_tag == completed_instruction.dest_tag)
                            rob_entry->is_completed = true;
                    }

                    if (completed_instruction.dest != -1)  // Else no need to search scheduling queue and messy registers for tag match
                    {
                        update_rs_and_messy_regs(completed_instruction);
                    }
                } 
                else 
                {
                    // Cache miss, so update cache
                    #ifdef DEBUG
                    printf("Got response from L2, updating dcache for read from instruction with tag %" PRIu64 " (dcache index: %" PRIx64 ", dcache tag: %" PRIx64 ")\n", LSU[i].stages.front().dest_tag, d_cache->generate_index(LSU[i].stages.front().memory_addr), d_cache->generate_tag(LSU[i].stages.front().memory_addr));
                    #endif
                    execution_instruction completed_instruction = LSU[i].complete_execution();
                    d_cache->insert(completed_instruction.memory_addr);
                    #ifdef DEBUG
                    if (completed_instruction.dest != -1)
                    {
                        printf("Instruction with tag %" PRIu64 " and dest reg=R%" PRId8 " completing\n", completed_instruction.dest_tag, completed_instruction.dest);
                    }
                    else
                    {
                        printf("Instruction with tag %" PRIu64 " and dest reg=(none) completing\n", completed_instruction.dest_tag);
                    }
                    #endif
                    #ifdef DEBUG
                    printf("Removing instruction with tag %" PRIu64 " from the scheduling queue\n", completed_instruction.dest_tag);
                    printf("Inserting instruction with tag %" PRIu64 " into the ROB\n", completed_instruction.dest_tag);
                    #endif
                    schedule_queue->complete(completed_instruction.dest_tag);
                    // Update ROB
                    for (auto rob_entry=rob.begin(); rob_entry!=rob.end(); rob_entry++)
                    {
                        if (rob_entry->dest_tag == completed_instruction.dest_tag)
                            rob_entry->is_completed = true;
                    }

                    if (completed_instruction.dest != -1)  // Else no need to search scheduling queue and messy registers for tag match
                    {
                        update_rs_and_messy_regs(completed_instruction);
                    } 
                } 
            }
            else
            {
                execution_instruction completed_instruction = LSU[i].complete_execution();
                #ifdef DEBUG
                if (completed_instruction.dest != -1)
                {
                    printf("Instruction with tag %" PRIu64 " and dest reg=R%" PRId8 " completing\n", completed_instruction.dest_tag, completed_instruction.dest);
                }
                else
                {
                    printf("Instruction with tag %" PRIu64 " and dest reg=(none) completing\n", completed_instruction.dest_tag);
                }
                #endif
                #ifdef DEBUG
                printf("Removing instruction with tag %" PRIu64 " from the scheduling queue\n", completed_instruction.dest_tag);
                printf("Inserting instruction with tag %" PRIu64 " into the ROB\n", completed_instruction.dest_tag);
                #endif
                schedule_queue->complete(completed_instruction.dest_tag);
                // Update ROB
                for (auto rob_entry=rob.begin(); rob_entry!=rob.end(); rob_entry++)
                {
                    if (rob_entry->dest_tag == completed_instruction.dest_tag)
                        rob_entry->is_completed = true;
                }

                if (completed_instruction.dest != -1)  // Else no need to search scheduling queue and messy registers for tag match
                {
                    update_rs_and_messy_regs(completed_instruction);
                } 
            }
        }
        else if (!LSU[i].stages.empty())
        {
            load_store_indices[d_cache->generate_index(LSU[i].stages.front().memory_addr)] = 1; 
        }
    }
    stats->rob_max_usage = get_rob_usage() > stats->rob_max_usage ? get_rob_usage() : stats->rob_max_usage;
    stats->rob_avg_usage += get_rob_usage();
}

// Optional helper function which is responsible for looking through the
// scheduling queue and firing instructions. Note that when multiple
// instructions are ready to fire in a given cycle, they must be fired in
// program order. Also, load and store instructions must be fired according to
// the modified TSO ordering described in the assignment PDF. Finally,
// instructions stay in their reservation station in the scheduling queue until
// they complete (at which point stage_exec() above should free their RS).
static void stage_schedule(procsim_stats_t *stats) {
    // TODO: fill me in
    bool fired_instruction = false;
    if (schedule_queue->hashmap.size() != 0)
    {
        for (auto it = schedule_queue->reservation_stations.begin(); it != schedule_queue->reservation_stations.end(); it++)
        {
            if (it->src1_ready and it->src2_ready and !it->is_fired)
            {
                switch (it->opcode)
                {
                    case OPCODE_ADD: case OPCODE_BRANCH:
                        for (uint64_t i = 0; i<A ;i++)
                        {
                            if (ALU[i].is_ready())
                            {
                                ALU[i].execute_instruction(it->dest, it->dest_tag, it->opcode, it->is_interrupt);
                                it->is_fired = true;
                                fired_instruction = true;
                                #ifdef DEBUG
                                printf("Firing instruction with tag %" PRIu64 "!\n", it->dest_tag);
                                #endif
                                break;
                            }
                        }
                        break;

                    case OPCODE_MUL:
                        for (uint64_t i = 0; i<M; i++)
                        {
                            if (MUL[i].is_ready())
                            {
                                MUL[i].execute_instruction(it->dest, it->dest_tag, it->opcode, it->is_interrupt);
                                it->is_fired = true;
                                fired_instruction = true;
                                #ifdef DEBUG
                                printf("Firing instruction with tag %" PRIu64 "!\n", it->dest_tag);
                                #endif
                                break;
                            }
                        }
                        break;

                    case OPCODE_LOAD: case OPCODE_STORE:
                        for (uint64_t i = 0; i<L; i++)
                        {
                            if ((load_store_indices.find(d_cache->generate_index(it->load_store_addr)) == load_store_indices.end() or load_store_indices[d_cache->generate_index(it->load_store_addr)] == 0) and LSU[i].is_ready())
                            {
                                LSU[i].num_cycles = 1;
                                LSU[i].execute_instruction(it->dest, it->dest_tag, it->opcode, it->is_interrupt, it->load_store_addr);
                                fired_instruction = true /*and (!it->is_interrupt)*/;
                                // fired_instruction = !it->is_interrupt;
                                it->is_fired = true;
                                #ifdef DEBUG
                                printf("Firing instruction with tag %" PRIu64 "!\n", it->dest_tag);
                                #endif
                                break;
                            }
                        }
                        break;
                }
            }
            if (it->opcode == OPCODE_LOAD or it->opcode==OPCODE_STORE)
                load_store_indices[d_cache->generate_index(it->load_store_addr)] = 1;
        }
    }

    if (!fired_instruction)
        stats->no_fire_cycles += 1;

}

// Optional helper function which looks through the dispatch queue, decodes
// instructions, and inserts them into the scheduling queue. Dispatch should
// not add an instruction to the scheduling queue unless there is space for it
// in the scheduling queue and the ROB; effectively, dispatch allocates
// reservation stations and ROB space for each instruction dispatched and
// stalls if there is either is unavailable. Note the scheduling queue and ROB
// have configurable sizes. The PDF has details.
static void stage_dispatch(procsim_stats_t *stats) {
    // TODO: fill me in
    // std::cout<<"OK Aight"<<std::endl;
    // std::list<inst_t>::iterator it;
    uint64_t free_slots = schedule_queue->num_of_free_slots();
        // std::cout<<"Wait a min" <<std::endl;
    while (dispatch_queue.size() > 0 and free_slots > 0 and rob.size() < max_rob_entries)
    {
        inst_t next_inst = dispatch_queue.front();
        dispatch_queue.pop_front();
        bool src2_exists = next_inst.src2 != -1;
        bool src1_exists = next_inst.src1 != -1;
        #ifdef DEBUG
        printf("Dispatching instruction ");
        const inst_t* ptr = &next_inst;
        print_instruction(ptr);
        #endif

        schedule_queue->add(next_inst, current_tag, src1_exists ? messy_regs.tags[next_inst.src1] : 0, src2_exists ? messy_regs.tags[next_inst.src2] : 0, src1_exists ? messy_regs.ready_bits[next_inst.src1]: true, src2_exists ? messy_regs.ready_bits[next_inst.src2] : true);
        rob.push_back(execution_instruction(next_inst.dest, current_tag, next_inst.load_store_addr, next_inst.opcode, next_inst.interrupt, false));
        
        if (next_inst.dest != -1)
        {
            #ifdef DEBUG
            printf("Marking ready=0 and assigning tag=%" PRIu64 " for R%" PRId8 " in messy RF\n", current_tag, next_inst.dest);
            #endif
            messy_regs.tags[next_inst.dest] = current_tag;
            messy_regs.ready_bits[next_inst.dest] = false;
        }
        current_tag += 1;
        free_slots -= 1;
    }
    if (rob.size() == max_rob_entries and free_slots > 0 and dispatch_queue.size() > 0)
        stats->rob_stall_cycles += 1;

    stats->schedq_max_usage = schedule_queue->reservation_stations.size() > stats->schedq_max_usage ? schedule_queue->reservation_stations.size() : stats->schedq_max_usage;
    stats->schedq_avg_usage += schedule_queue->reservation_stations.size();
}

// Optional helper function which fetches instructions from the instruction
// cache using the provided procsim_driver_read_inst() function implemented
// in the driver and appends them to the dispatch queue. To simplify the
// project, the dispatch queue is infinite in size.
static void stage_fetch(procsim_stats_t *stats) {
    // TODO: fill me in
    for (uint64_t i=0;i<fetch_rate; i++)
    {
        const inst_t* next_instr = procsim_driver_read_inst();
        if (next_instr == NULL)
            break;
        #ifdef DEBUG
        printf("Fetching instruction ");
        print_instruction(next_instr);
        #endif

        dispatch_queue.push_back(*next_instr);
        stats->instructions_fetched += 1;
        #ifdef DEBUG
        printf(". Adding to dispatch queue\n");
        #endif
    }
    stats->dispq_max_usage = dispatch_queue.size() > stats->dispq_max_usage ? dispatch_queue.size() : stats->dispq_max_usage;
    stats->dispq_avg_usage += dispatch_queue.size();
}

// Use this function to initialize all your data structures, simulator
// state, and statistics.
void procsim_init(const procsim_conf_t *sim_conf, procsim_stats_t *stats) {
    // TODO: fill me in

    #ifdef DEBUG
    printf("\nScheduling queue capacity: %lu instructions\n", sim_conf->num_schedq_entries_per_fu*(sim_conf->num_alu_fus+sim_conf->num_mul_fus+sim_conf->num_lsu_fus)); // TODO: fix me
    printf("Initial messy RF state:\n");
    print_messy_rf();
    printf("\n");
    #endif
    d_cache = new data_cache(sim_conf->dcache_c, 6);
    A = sim_conf->num_alu_fus;
    M = sim_conf->num_mul_fus;
    L = sim_conf->num_lsu_fus;
    S = sim_conf->num_schedq_entries_per_fu*(A+M+L);
    schedule_queue = new scheduling_queue(S);
    messy_regs = register_file();
    ALU = std::vector<functional_unit>(A, functional_unit(false, 1));
    LSU = std::vector<functional_unit>(L, functional_unit(false, 1));
    MUL = std::vector<functional_unit>(M, functional_unit(true, 3));
    W = sim_conf->retire_width;
    max_rob_entries = sim_conf->num_rob_entries;
    fetch_rate = sim_conf->fetch_width;
}

// To avoid confusion, we have provided this function for you. Notice that this
// calls the stage functions above in reverse order! This is intentional and
// allows you to avoid having to manage pipeline registers between stages by
// hand. This function returns the number of instructions retired, and also
// returns if an interrupt was retired by assigning true or false to
// *retired_interrupt_out, an output parameter.
uint64_t procsim_do_cycle(procsim_stats_t *stats,
                          bool *retired_interrupt_out) {
    #ifdef DEBUG
    printf("================================ Begin cycle %" PRIu64 " ================================\n", stats->cycles);
    #endif

    // stage_state_update() should set *retired_interrupt_out for us
    uint64_t retired_this_cycle = stage_state_update(stats, retired_interrupt_out);

    if (*retired_interrupt_out) {
        #ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Retired interrupt, so flushing pipeline!\n", retired_this_cycle);
        #endif

        // After we retire an interrupt, flush the pipeline immediately and
        // then pick up where we left off in the next cycle.
        stats->interrupts++;
        flush_pipeline();
    } else {
        #ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Did not retire interrupt, so proceeding with other pipeline stages.\n", retired_this_cycle);
        #endif

        // If we didn't retire an interupt, then continue simulating the other
        // pipeline stages
        stage_exec(stats);
        stage_schedule(stats);
        stage_dispatch(stats);
        stage_fetch(stats);
    }

    #ifdef DEBUG
        printf("End-of-cycle dispatch queue usage: %lu\n", dispatch_queue.size()); // TODO: fix me
        printf("End-of-cycle messy RF state:\n");
        print_messy_rf();
        printf("End-of-cycle scheduling queue state:\n");
        print_schedq();
        printf("End-of-cycle ROB state:\n");
        print_rob();
        printf("================================ End cycle %" PRIu64 " ================================\n\n", stats->cycles);
    #endif

    // TODO: Increment max_usages and avg_usages in stats here!
    stats->cycles++;

    // Return the number of instructions we retired this cycle (including the
    // interrupt we retired, if there was one!)
    return retired_this_cycle;
}

// Use this function to free any memory allocated for your simulator and to
// calculate some final statistics.
void procsim_finish(procsim_stats_t *stats) {
    // TODO: fill me in
    stats->dcache_read_miss_ratio = stats->dcache_read_misses/double(stats->dcache_reads);
    stats->dcache_read_aat = 1 + 9*stats->dcache_read_miss_ratio;
    stats->dispq_avg_usage /= double(stats->cycles);
    stats->schedq_avg_usage /= double(stats->cycles);
    stats->rob_avg_usage /= double(stats->cycles);
    stats->ipc = stats->instructions_retired / double(stats->cycles);

    delete d_cache;
    delete schedule_queue;
}
