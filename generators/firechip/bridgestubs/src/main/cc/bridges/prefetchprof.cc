// See LICENSE for license details

#include "prefetchprof.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>

char prefetchprof_t::KIND;

prefetchprof_t::prefetchprof_t(simif_t &sim,
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
                               unsigned stream_depth)
    : streaming_bridge_driver_t(sim, engine, &KIND), mmio_addrs(mmio_addrs),
      prefetchprofno(prefetchprofno), events(events), clock_info(clock_info),
      hist_bins(hist_bins), line_bytes(line_bytes), filter_tags(filter_tags), stream_idx(stream_idx),
      stream_depth(stream_depth) {
  // Results are written to <base><id>.csv; the base defaults to PREFETCHPROF.
  const std::string filename_arg = "+prefetchprof-filename-base=";
  std::string base = "PREFETCHPROF";
  for (const auto &arg : args) {
    if (arg.find(filename_arg) == 0) {
      base = arg.substr(filename_arg.length());
    }
  }
  output_filename = base + std::to_string(prefetchprofno) + ".csv";
  hist_filename = base + std::to_string(prefetchprofno) + "_addr_hist.csv";

  trace_filename = base + std::to_string(prefetchprofno) + "_trace.csv";
  for (const auto &arg : args) {
    if (arg == "+prefetch_trace") {
      trace_enabled = true;
    }
  }

  // Filter: the tags to keep active, comma separated; every tag by default.
  filter_mask = filter_tags.size() >= 32 ? 0xffffffffu
                                         : (1u << filter_tags.size()) - 1;
  const std::string filter_arg = "+prefetchprof-filter=";
  for (const auto &arg : args) {
    if (arg.find(filter_arg) != 0) {
      continue;
    }
    if (filter_tags.empty()) {
      throw std::runtime_error(
          "PrefetchProf: +prefetchprof-filter given, but no event in this "
          "build carries a filter tag (tag them with WithPrefetchProfTag)");
    }
    filter_mask = 0;
    std::string list = arg.substr(filter_arg.length());
    size_t pos = 0;
    while (pos <= list.size()) {
      size_t comma = list.find(',', pos);
      if (comma == std::string::npos) {
        comma = list.size();
      }
      const std::string name = list.substr(pos, comma - pos);
      pos = comma + 1;
      if (name.empty()) {
        continue;
      }
      bool found = false;
      for (size_t i = 0; i < filter_tags.size(); i++) {
        if (filter_tags[i] == name) {
          filter_mask |= 1u << i;
          found = true;
        }
      }
      if (!found) {
        std::string valid;
        for (const auto &t : filter_tags) {
          valid += (valid.empty() ? "" : ", ") + t;
        }
        throw std::runtime_error("PrefetchProf: unknown filter tag '" + name +
                                 "'. Tags in this build: " + valid);
      }
    }
  }

  // Address histogram granularity: one bin covers 2^shift bytes.
  const std::string shift_arg = "+prefetchprof-hist-shift=";
  for (const auto &arg : args) {
    if (arg.find(shift_arg) == 0) {
      hist_shift = std::stoul(arg.substr(shift_arg.length()));
    }
  }
}

void prefetchprof_t::init() {
  write(mmio_addrs.filter_mask, filter_mask);
  if (!filter_tags.empty()) {
    std::string active;
    for (size_t i = 0; i < filter_tags.size(); i++) {
      if (filter_mask & (1u << i)) {
        active += (active.empty() ? "" : ", ") + filter_tags[i];
      }
    }
    fprintf(stdout,
            "[PrefetchProf %u] filter: recording tags {%s} of {%zu tags in the build}\n",
            prefetchprofno,
            active.empty() ? "none" : active.c_str(),
            filter_tags.size());
  }
  write(mmio_addrs.hist_shift, hist_shift);
  if (trace_enabled) {
    if (!read(mmio_addrs.trace_available)) {
      throw std::runtime_error(
          "PrefetchProf: trace requested, but this build does not export the "
          "pf_issue / l1d_pf_fill / core_pc events (needs a prefetching "
          "config with a non-blocking L1D)");
    }
    trace_file.open(trace_filename, std::ofstream::out);
    if (!trace_file.is_open()) {
      throw std::runtime_error("Could not open output file: " +
                               trace_filename);
    }
    trace_file << "cycle,pc,event,addr\n";
    write(mmio_addrs.trace_enable, 1);
  }
  // Allow the bridge to start consuming tokens.
  write(mmio_addrs.init_done, 1);
}

void prefetchprof_t::tick() {
  if (trace_enabled) {
    drain_trace(false);
  }
}

bool prefetchprof_t::drain_trace(bool flush) {
  // One 64B stream token per record: cycle, pc, issue addr, fill addr, flags.
  constexpr size_t record_bytes = STREAM_WIDTH_BYTES;
  const size_t batch = stream_depth / 2 ? stream_depth / 2 : 1;
  std::vector<uint64_t> buf(batch * record_bytes / sizeof(uint64_t));
  bool any = false;
  while (true) {
    size_t got = pull(stream_idx,
                      buf.data(),
                      batch * record_bytes,
                      flush ? 0 : batch * record_bytes);
    size_t records = got / record_bytes;
    for (size_t i = 0; i < records; i++) {
      const uint64_t *r = &buf[i * (record_bytes / sizeof(uint64_t))];
      const uint64_t cycle = r[0], pc = r[1], flags = r[4];
      if (flags & 1) {
        trace_file << cycle << ",0x" << std::hex << pc << std::dec
                   << ",issue,0x" << std::hex << r[2] << std::dec << "\n";
      }
      if (flags & 2) {
        trace_file << cycle << ",0x" << std::hex << pc << std::dec
                   << ",fill,0x" << std::hex << r[3] << std::dec << "\n";
      }
    }
    trace_records += records;
    any = any || records > 0;
    // Outside of flush, one batch per tick keeps the simulation moving.
    if (!flush || records == 0) {
      break;
    }
  }
  return any;
}

uint64_t prefetchprof_t::read_cycles() {
  uint64_t low = read(mmio_addrs.cycles_low);
  uint64_t high = read(mmio_addrs.cycles_high);
  return (high << 32) | low;
}

uint64_t prefetchprof_t::read_instr_ret() {
  uint64_t low = read(mmio_addrs.instr_ret_low);
  uint64_t high = read(mmio_addrs.instr_ret_high);
  return (high << 32) | low;
}

prefetchprof_t::AccuracyStats prefetchprof_t::read_accuracy() {
  auto rd = [this](uint64_t lo, uint64_t hi) {
    uint64_t low = read(lo);
    uint64_t high = read(hi);
    return (high << 32) | low;
  };
  AccuracyStats a;
  a.fills = rd(mmio_addrs.acc_fills_low, mmio_addrs.acc_fills_high);
  a.useful = rd(mmio_addrs.acc_useful_low, mmio_addrs.acc_useful_high);
  a.evicted_used =
      rd(mmio_addrs.acc_evicted_used_low, mmio_addrs.acc_evicted_used_high);
  a.evicted_unused =
      rd(mmio_addrs.acc_evicted_unused_low, mmio_addrs.acc_evicted_unused_high);
  a.lost = rd(mmio_addrs.acc_lost_low, mmio_addrs.acc_lost_high);
  a.demands = rd(mmio_addrs.acc_demands_low, mmio_addrs.acc_demands_high);
  a.timely = rd(mmio_addrs.acc_timely_low, mmio_addrs.acc_timely_high);
  a.late = rd(mmio_addrs.acc_late_low, mmio_addrs.acc_late_high);
  a.demand_misses =
      rd(mmio_addrs.acc_demand_misses_low, mmio_addrs.acc_demand_misses_high);
  a.demand_misses_secondary =
      rd(mmio_addrs.acc_demand_misses_secondary_low,
         mmio_addrs.acc_demand_misses_secondary_high);
  a.pf_issues = rd(mmio_addrs.acc_pf_issues_low, mmio_addrs.acc_pf_issues_high);
  a.mem_in_bytes =
      rd(mmio_addrs.acc_mem_in_bytes_low, mmio_addrs.acc_mem_in_bytes_high);
  a.mem_out_bytes =
      rd(mmio_addrs.acc_mem_out_bytes_low, mmio_addrs.acc_mem_out_bytes_high);
  a.mem_acquires =
      rd(mmio_addrs.acc_mem_acquires_low, mmio_addrs.acc_mem_acquires_high);
  a.mem_ctrl_msgs =
      rd(mmio_addrs.acc_mem_ctrl_msgs_low, mmio_addrs.acc_mem_ctrl_msgs_high);
  return a;
}

std::vector<uint64_t> prefetchprof_t::read_addr_histogram() {
  std::vector<uint64_t> bins(hist_bins);
  for (unsigned i = 0; i < hist_bins; i++) {
    write(mmio_addrs.hist_index, i);
    uint64_t low = read(mmio_addrs.hist_data_low);
    uint64_t high = read(mmio_addrs.hist_data_high);
    bins[i] = (high << 32) | low;
  }
  return bins;
}

void prefetchprof_t::finish() {
  if (trace_enabled) {
    drain_trace(true);
    trace_file.close();
    fprintf(stdout,
            "[PrefetchProf %u] trace: %" PRIu64 " records -> %s\n",
            prefetchprofno,
            trace_records,
            trace_filename.c_str());
  }
  // Read the final counts into software now that the simulation is over.
  instr_ret = read_instr_ret();
  accuracy = read_accuracy();
  fprintf(stdout,
          "[PrefetchProf %u] observed %" PRIu64 " target cycles (%zu events)\n",
          prefetchprofno,
          read_cycles(),
          events.size());
  fprintf(stdout, "[PrefetchProf %u]   instr_ret = %" PRIu64 "\n", prefetchprofno, instr_ret);

  // Ratios are NaN when their denominator is 0 (printed as n/a / nan).
  auto ratio = [](uint64_t num, uint64_t den) {
    return den ? (double)num / den : std::nan("");
  };
  auto show = [](double v) {
    char buf[32];
    if (std::isnan(v)) {
      snprintf(buf, sizeof(buf), "n/a");
    } else {
      snprintf(buf, sizeof(buf), "%.4f", v);
    }
    return std::string(buf);
  };
  // Accuracy over all prefetched blocks, and over those whose L1D lifetime has ended.
  const uint64_t closed = accuracy.evicted_used + accuracy.evicted_unused;
  const double acc_all = ratio(accuracy.useful, accuracy.fills);
  const double acc_closed = ratio(accuracy.evicted_used, closed);
  // Of the useful prefetches, how many had arrived by first use, and how many
  // the demand caught up with while still in flight.
  const double timeliness = ratio(accuracy.timely, accuracy.useful);
  const double lateness = ratio(accuracy.late, accuracy.useful);
  fprintf(stdout,
          "[PrefetchProf %u]   prefetch fills=%" PRIu64 " useful=%" PRIu64
          " (accuracy %s); evicted used=%" PRIu64 " unused=%" PRIu64
          " (accuracy over completed lifetimes %s); lost=%" PRIu64 "\n",
          prefetchprofno,
          accuracy.fills,
          accuracy.useful,
          show(acc_all).c_str(),
          accuracy.evicted_used,
          accuracy.evicted_unused,
          show(acc_closed).c_str(),
          accuracy.lost);
  // Miss rate, and prefetch traffic relative to demand accesses: prefetch
  // requests issued per demand access, and prefetch fills (lines actually
  // fetched) per demand access. Total misses count lines fetched by prefetches
  // too.
  const double miss_rate = ratio(accuracy.demand_misses, accuracy.demands);
  // Misses that start a new line fetch (the rest merged into one already in flight).
  const uint64_t primary_misses =
      accuracy.demand_misses - accuracy.demand_misses_secondary;
  const double primary_miss_rate = ratio(primary_misses, accuracy.demands);
  const double issues_per_access = ratio(accuracy.pf_issues, accuracy.demands);
  const double fills_per_access = ratio(accuracy.fills, accuracy.demands);
  const uint64_t total_misses = accuracy.demand_misses + accuracy.fills;
  fprintf(stdout,
          "[PrefetchProf %u]   L1D demand accesses=%" PRIu64 " demand misses=%" PRIu64
          " (primary %" PRIu64 ", secondary %" PRIu64
          ", miss rate %s, primary miss rate %s); prefetch fills=%" PRIu64
          "; total L1D misses=%" PRIu64 "\n",
          prefetchprofno,
          accuracy.demands,
          accuracy.demand_misses,
          primary_misses,
          accuracy.demand_misses_secondary,
          show(miss_rate).c_str(),
          show(primary_miss_rate).c_str(),
          accuracy.fills,
          total_misses);
  // Traffic across the L1D boundary. Prefetch-fetched bytes are the prefetch
  // fills times the line size; the rest of the inbound bytes were fetched by
  // demand misses.
  const uint64_t mem_total_bytes = accuracy.mem_in_bytes + accuracy.mem_out_bytes;
  const uint64_t pf_fetch_bytes = accuracy.fills * line_bytes;
  const double pf_share = ratio(pf_fetch_bytes, accuracy.mem_in_bytes);
  const double bytes_per_instr = ratio(mem_total_bytes, instr_ret);
  fprintf(stdout,
          "[PrefetchProf %u]   L1D traffic: in=%" PRIu64 " B (%" PRIu64
          " lines) out=%" PRIu64 " B (%" PRIu64 " lines) total=%" PRIu64
          " B; prefetch-fetched=%" PRIu64
          " B (%s of inbound); acquires=%" PRIu64 " ctrl msgs=%" PRIu64
          "; bytes/instr %s\n",
          prefetchprofno,
          accuracy.mem_in_bytes,
          line_bytes ? accuracy.mem_in_bytes / line_bytes : 0,
          accuracy.mem_out_bytes,
          line_bytes ? accuracy.mem_out_bytes / line_bytes : 0,
          mem_total_bytes,
          pf_fetch_bytes,
          show(pf_share).c_str(),
          accuracy.mem_acquires,
          accuracy.mem_ctrl_msgs,
          show(bytes_per_instr).c_str());
  fprintf(stdout,
          "[PrefetchProf %u]   prefetch requests=%" PRIu64
          " per demand access: issued %s, fetched %s\n",
          prefetchprofno,
          accuracy.pf_issues,
          show(issues_per_access).c_str(),
          show(fills_per_access).c_str());
  fprintf(stdout,
          "[PrefetchProf %u]   timely=%" PRIu64 " late=%" PRIu64
          " of %" PRIu64 " useful (timeliness %s, lateness %s)\n",
          prefetchprofno,
          accuracy.timely,
          accuracy.late,
          accuracy.useful,
          show(timeliness).c_str(),
          show(lateness).c_str());

  std::ofstream out(output_filename, std::ofstream::out);
  if (!out.is_open()) {
    throw std::runtime_error("Could not open output file: " + output_filename);
  }
  out << "label,count\n";
  out << "cycles," << read_cycles() << "\n";
  out << "filter_mask," << filter_mask << "\n";
  out << "instr_ret," << instr_ret << "\n";
  out << "pf_fills," << accuracy.fills << "\n";
  out << "pf_useful," << accuracy.useful << "\n";
  out << "pf_evicted_used," << accuracy.evicted_used << "\n";
  out << "pf_evicted_unused," << accuracy.evicted_unused << "\n";
  out << "pf_lost," << accuracy.lost << "\n";
  out << "l1d_demands," << accuracy.demands << "\n";
  out << "pf_timely," << accuracy.timely << "\n";
  out << "pf_late," << accuracy.late << "\n";
  out << "l1d_demand_misses," << accuracy.demand_misses << "\n";
  out << "l1d_demand_misses_secondary," << accuracy.demand_misses_secondary << "\n";
  out << "l1d_total_misses," << total_misses << "\n";
  out << "pf_issues," << accuracy.pf_issues << "\n";
  out << "l1d_demand_misses_primary," << primary_misses << "\n";
  out << "l1d_miss_rate," << miss_rate << "\n";
  out << "l1d_primary_miss_rate," << primary_miss_rate << "\n";
  out << "pf_issued_per_access," << issues_per_access << "\n";
  out << "pf_fetched_per_access," << fills_per_access << "\n";
  out << "mem_in_bytes," << accuracy.mem_in_bytes << "\n";
  out << "mem_out_bytes," << accuracy.mem_out_bytes << "\n";
  out << "mem_total_bytes," << mem_total_bytes << "\n";
  out << "mem_acquires," << accuracy.mem_acquires << "\n";
  out << "mem_ctrl_msgs," << accuracy.mem_ctrl_msgs << "\n";
  out << "pf_fetch_bytes," << pf_fetch_bytes << "\n";
  out << "pf_share_of_inbound," << pf_share << "\n";
  out << "mem_bytes_per_instr," << bytes_per_instr << "\n";
  out << "pf_accuracy," << acc_all << "\n";
  out << "pf_accuracy_completed," << acc_closed << "\n";
  out << "pf_timeliness," << timeliness << "\n";
  out << "pf_lateness," << lateness << "\n";

  // Address histogram: only non-empty bins. A bin covers the 2^shift bytes at
  // (bin << shift) (+ k * hist_bins << shift for any k, since bins wrap).
  std::ofstream hist_out(hist_filename, std::ofstream::out);
  if (!hist_out.is_open()) {
    throw std::runtime_error("Could not open output file: " + hist_filename);
  }
  hist_out << "# bins=" << hist_bins << " shift=" << hist_shift << "\n";
  hist_out << "bin,base_offset,count\n";
  const std::vector<uint64_t> hist = read_addr_histogram();
  for (unsigned i = 0; i < hist_bins; i++) {
    if (hist[i] != 0) {
      hist_out << i << ",0x" << std::hex << ((uint64_t)i << hist_shift)
               << std::dec << "," << hist[i] << "\n";
    }
  }
}
