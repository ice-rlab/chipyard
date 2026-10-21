// See LICENSE for license details

#ifndef __PREFETCHPROF_H
#define __PREFETCHPROF_H

#include "core/bridge_driver.h"
#include "core/clock_info.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

/**
 * Structure carrying the addresses of all fixed MMIO registers. The member
 * order must match the register order in PrefetchProfBridgeModule.
 */
struct PREFETCHPROFBRIDGEMODULE_struct {
  uint64_t init_done;
  uint64_t filter_mask;
  uint64_t cycles_low;
  uint64_t cycles_high;
  uint64_t instr_ret_low;
  uint64_t instr_ret_high;
  uint64_t hist_shift;
  uint64_t hist_index;
  uint64_t hist_data_low;
  uint64_t hist_data_high;
  uint64_t acc_fills_low;
  uint64_t acc_fills_high;
  uint64_t acc_useful_low;
  uint64_t acc_useful_high;
  uint64_t acc_evicted_used_low;
  uint64_t acc_evicted_used_high;
  uint64_t acc_evicted_unused_low;
  uint64_t acc_evicted_unused_high;
  uint64_t acc_lost_low;
  uint64_t acc_lost_high;
  uint64_t acc_demands_low;
  uint64_t acc_demands_high;
  uint64_t acc_timely_low;
  uint64_t acc_timely_high;
  uint64_t acc_late_low;
  uint64_t acc_late_high;
  uint64_t acc_demand_misses_low;
  uint64_t acc_demand_misses_high;
  uint64_t acc_demand_misses_secondary_low;
  uint64_t acc_demand_misses_secondary_high;
  uint64_t acc_pf_issues_low;
  uint64_t acc_pf_issues_high;
  uint64_t acc_mem_in_bytes_low;
  uint64_t acc_mem_in_bytes_high;
  uint64_t acc_mem_out_bytes_low;
  uint64_t acc_mem_out_bytes_high;
  uint64_t acc_mem_acquires_low;
  uint64_t acc_mem_acquires_high;
  uint64_t acc_mem_ctrl_msgs_low;
  uint64_t acc_mem_ctrl_msgs_high;
  uint64_t trace_available;
  uint64_t trace_enable;
};

/**
 * Driver for the PrefetchProf bridge, which will be used to analyze prefetch
 * accuracy, window and timeliness. Currently a skeleton: it performs the init
 * handshake and reports how many target cycles the bridge observed.
 */
class prefetchprof_t : public streaming_bridge_driver_t {
public:
  /// The identifier for the bridge type used for casts.
  static char KIND;

  /// An annotated target-side event wired into the bridge.
  struct Event {
    std::string label;
    std::string description;
    uint32_t bit_width;
  };

  prefetchprof_t(simif_t &sim,
                 StreamEngine &engine,
                 const PREFETCHPROFBRIDGEMODULE_struct &mmio_addrs,
                 unsigned prefetchprofno,
                 const std::vector<std::string> &args,
                 const std::vector<Event> &events,
                 const ClockInfo &clock_info,
                 unsigned hist_bins,
                 unsigned line_bytes,
                 const std::vector<std::string> &filter_tags,
                 unsigned stream_idx,
                 unsigned stream_depth);
  ~prefetchprof_t() override = default;

  void init() override;
  void tick() override;
  void finish() override;

private:
  const PREFETCHPROFBRIDGEMODULE_struct mmio_addrs;
  const unsigned prefetchprofno;
  std::vector<Event> events;
  ClockInfo clock_info;

  std::string output_filename;
  std::string hist_filename;
  unsigned hist_bins;
  unsigned line_bytes; ///< cache line size, in bytes

  // Filter tags (+prefetchprof-filter=a,b): events carrying a tag are only
  // recorded while it is selected. All tags are selected by default.
  std::vector<std::string> filter_tags;
  uint32_t filter_mask = 0;
  unsigned hist_shift = 6; ///< log2 of the bytes covered by one bin

  // Trace mode (+prefetch_trace): every prefetch with its cycle
  // and PC, streamed from the bridge. Off by default; counters run either way.
  bool trace_enabled = false;
  unsigned stream_idx;
  unsigned stream_depth;
  std::string trace_filename;
  std::ofstream trace_file;
  uint64_t trace_records = 0;

  /// Drains the trace stream into the trace file. With \c flush, also takes
  /// whatever partial batch is left; returns true if any record was read.
  bool drain_trace(bool flush);

  /// Reads the whole address histogram from the bridge.
  std::vector<uint64_t> read_addr_histogram();

  uint64_t read_cycles();
  uint64_t read_instr_ret();

public:
  /// Prefetch accuracy counters, read from the bridge when the simulation ends.
  struct AccuracyStats {
    uint64_t fills = 0;          ///< prefetched blocks brought into the L1D
    uint64_t useful = 0;         ///< prefetched blocks the core demanded at least once
    uint64_t evicted_used = 0;   ///< prefetched blocks evicted after being used
    uint64_t evicted_unused = 0; ///< prefetched blocks evicted without ever being used
    uint64_t lost = 0;           ///< prefetched blocks the tracking table had to drop
    uint64_t demands = 0;        ///< demand accesses seen by the L1D
    uint64_t timely = 0;         ///< useful prefetches whose line had arrived at first use
    uint64_t late = 0;           ///< useful prefetches the demand caught up with in flight
    uint64_t demand_misses = 0;  ///< L1D demand misses (cacheable, one per miss)
    uint64_t demand_misses_secondary = 0; ///< of those, merged into an in-flight fetch
    uint64_t pf_issues = 0;      ///< prefetch requests accepted by the L1D
    uint64_t mem_in_bytes = 0;   ///< bytes of cache lines fetched into the L1D
    uint64_t mem_out_bytes = 0;  ///< bytes of dirty lines written back
    uint64_t mem_acquires = 0;   ///< Acquire requests sent to the next level
    uint64_t mem_ctrl_msgs = 0;  ///< Release/ProbeAck messages without data
  };
  const AccuracyStats &get_accuracy() const { return accuracy; }

private:
  AccuracyStats accuracy;
  AccuracyStats read_accuracy();

  /// Retired instructions, read from the bridge when the simulation ends.
  uint64_t instr_ret = 0;
public:
  uint64_t get_instr_ret() const { return instr_ret; }
};

#endif // __PREFETCHPROF_H
