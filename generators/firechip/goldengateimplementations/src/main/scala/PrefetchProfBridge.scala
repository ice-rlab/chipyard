// See LICENSE for license details

package firechip.goldengateimplementations

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.Parameters
import freechips.rocketchip.util._

import midas.widgets._
import firesim.lib.bridgeutils._
import firechip.bridgeinterfaces._

/** Host-side module of the PrefetchProf bridge.
  *
  * Currently a skeleton: it sinks the target tokens, counts the target cycles it has seen, and exposes that count and
  * an init handshake to the driver. Prefetch accuracy / window / timeliness logic will be added here, consuming
  * `hPort.hBits.events`.
  */
class PrefetchProfBridgeModule(key: PrefetchProfKey)(implicit p: Parameters)
    extends BridgeModule[HostPortIO[PrefetchProfTargetIO]]()(p)
    with StreamToHostCPU {

  // Depth of the stream to the driver, in 64B records. Only used in trace mode (+prefetch_trace).
  val toHostCPUQueueDepth = 1024

  lazy val module = new BridgeModuleImp(this) {
    val io    = IO(new WidgetIO())
    val hPort = IO(HostPort(new PrefetchProfTargetIO(key.eventMetadata, key.triggerName, key.resetPortName)))

    // Driver sets this once it has finished init(); the bridge does not consume tokens before then.
    val initDone = genWORegInit(Wire(Bool()), "init_done", false.B)

    // ---- Filter tags -----------------------------------------------------------------------------------------------
    // An annotation can carry a tag (see midas.targetutils.PrefetchProfTarget), for example the prefetcher it belongs
    // to. The driver selects the active tags with +prefetchprof-filter=<tag>[,<tag>...] by writing filter_mask, one bit
    // per tag in the order of `filterTags` (all active by default). An event whose tag is not active reads as zero
    // everywhere in the bridge: counters, the accuracy tracking and the trace. Untagged events are always active.
    val filterTags = key.eventMetadata.map(_.tag).filter(_.nonEmpty).distinct.sorted
    val maskBits   = p(CtrlNastiKey).dataBits
    require(filterTags.length <= maskBits, s"PrefetchProf: ${filterTags.length} filter tags, at most $maskBits are supported")
    val filterMask = genWORegInit(Wire(UInt(maskBits.W)), "filter_mask", ((BigInt(1) << filterTags.length) - 1).U)
    def tagActive(tag: String): Bool = if (tag.isEmpty) true.B else filterMask(filterTags.indexOf(tag))

    // The annotated event signals, after the filter. Each is gated by the target reset on the target side, and is a UInt
    // of the annotated signal's width.
    val eventSignals: Seq[UInt] =
      key.eventMetadata.zip(hPort.hBits.events.map(_._2)).map { case (m, sig) =>
        Mux(tagActive(m.tag), sig, 0.U(sig.getWidth.W))
      }

    // Annotated events by label, i.e. the `label` given to midas.targetutils.PrefetchProfTarget in the target RTL.
    val eventsByLabel: Map[String, UInt] =
      key.eventMetadata.map(_.label).zip(eventSignals).toMap

    /** Returns the signal annotated with `label`. Fails elaboration if no such label was annotated, so a typo or a
      * missing annotation is caught at build time instead of silently reading zero.
      */
    def event(label: String): UInt =
      eventsByLabel.getOrElse(
        label,
        throw new RuntimeException(
          s"PrefetchProf: no event labeled '$label'. Annotated labels: ${eventsByLabel.keys.mkString(", ")}"
        ),
      )

    // Annotated sequences by group name (see midas.targetutils.PrefetchProfVecTarget), in their original order.
    val eventVecsByGroup: Map[String, Seq[UInt]] =
      key.eventMetadata
        .zip(eventSignals)
        .collect { case (m, sig) if m.vecGroup.isDefined => (m.vecGroup.get, m.vecIndex, sig) }
        .groupBy(_._1)
        .map { case (group, elems) => group -> elems.sortBy(_._2).map(_._3) }

    /** Returns the sequence annotated with `label` via PrefetchProfVecTarget, in annotation order. Fails elaboration if
      * there is none.
      */
    def eventVec(label: String): Seq[UInt] =
      eventVecsByGroup.getOrElse(
        label,
        throw new RuntimeException(
          s"PrefetchProf: no event sequence labeled '$label'. Annotated sequences: ${eventVecsByGroup.keys.mkString(", ")}"
        ),
      )

    /** Same, but yields an empty sequence when the label was not annotated. */
    def eventVecOrEmpty(label: String): Seq[UInt] = eventVecsByGroup.getOrElse(label, Seq())

    /** Same, but yields 0 when the label was not annotated. Useful while bringing up annotations one at a time. */
    def eventOrZero(label: String): UInt = eventsByLabel.getOrElse(label, 0.U)

    /** The lanes of an event: the elements of a PrefetchProfVecTarget sequence in order, or the single signal of a
      * scalar PrefetchProfTarget, or nothing if the label isn't annotated. A core with several memory ports annotates
      * each per-port signal once as a sequence; a single-ported core annotates it as a scalar. The logic below handles
      * any number of lanes active in the same cycle.
      */
    def lanes(label: String): Seq[UInt] =
      eventVecsByGroup.getOrElse(label, eventsByLabel.get(label).toSeq)
    def hasLabel(label: String): Boolean = lanes(label).nonEmpty
    def active(sig: UInt): Bool          = sig =/= 0.U


    // Address histogram storage: one 64-bit counter per bin, indexed by a slice of the (shifted) address. Memories
    // power up with undefined contents, so the bins are zeroed before the bridge starts consuming tokens. A core with
    // several memory ports gets one memory per port (so every access is counted in the cycle it happens); readout sums
    // them.
    val histBinsLog2 = 10
    val histBins     = 1 << histBinsLog2
    val dmemFires    = lanes("dmem_req_fire")
    val dmemAddrs    = lanes("dmem_addr")
    require(
      dmemFires.length == dmemAddrs.length,
      s"PrefetchProf: dmem_req_fire has ${dmemFires.length} lanes but dmem_addr has ${dmemAddrs.length}"
    )
    val hists        = Seq.fill(dmemFires.length)(Mem(histBins, UInt(64.W)))
    val clearIdx     = RegInit(0.U((histBinsLog2 + 1).W))
    val clearDone    = clearIdx(histBinsLog2)
    when(!clearDone) {
      hists.foreach(_.write(clearIdx(histBinsLog2 - 1, 0), 0.U))
      clearIdx := clearIdx + 1.U
    }

    // In trace mode the target stalls while the trace stream is full, so no record is dropped. Driven further down.
    val traceReady = Wire(Bool())

    val tFireHelper = DecoupledHelper(hPort.toHost.hValid, hPort.fromHost.hReady, initDone, clearDone, traceReady)
    val targetFire  = tFireHelper.fire()
    // We only sink tokens, so tie off the return channel
    hPort.fromHost.hValid := true.B
    hPort.toHost.hReady   := targetFire

    // The global trigger: the same window that gates AutoCounter, driven by TracerV's trigger source (so the TracerV
    // flags +trace-select / +trace-start / +trace-end apply here). Event counting, the histogram, the accuracy
    // tracker and the trace only record while it is high; the cycle counters below always run.
    val triggered = hPort.hBits.triggerEnable

    // Target cycles observed by this bridge (not counted under global reset).
    val cycles = RegInit(0.U(64.W))
    when(targetFire && !hPort.hBits.underGlobalReset) {
      cycles := cycles + 1.U
    }

    val cyclesLowWidth = math.min(64, p(CtrlNastiKey).dataBits)
    attach(cycles(cyclesLowWidth - 1, 0), "cycles_low", ReadOnly)
    attach(cycles >> cyclesLowWidth, "cycles_high", ReadOnly)

    // Labels without which the bridge cannot be built.
    val requiredLabels = Seq("instr_ret", "dmem_req_fire", "dmem_addr")
    requiredLabels.foreach { l =>
      require(
        hasLabel(l),
        s"PrefetchProf: required event '$l' is not annotated. Annotated: ${key.eventMetadata.map(_.label).mkString(", ")}"
      )
    }

    // Accumulate the annotated "instr_ret" event: adds its value (over all lanes) on each non-reset target cycle, so a
    // 1-bit signal counts occurrences and a wider one counts several per cycle. The driver reads it after the
    // simulation ends.
    val recording     = targetFire && !hPort.hBits.underGlobalReset && triggered
    val instrRetCount = RegInit(0.U(64.W))
    when(recording) {
      instrRetCount := instrRetCount + lanes("instr_ret").map(_.pad(64)).reduce(_ + _)
    }
    attach(instrRetCount(cyclesLowWidth - 1, 0), "instr_ret_low", ReadOnly)
    attach(instrRetCount >> cyclesLowWidth, "instr_ret_high", ReadOnly)

    // Histogram of D-cache request addresses. A request increments bin ((addr >> histShift) mod histBins), so bins
    // hold histBins consecutive (1 << histShift)-byte regions and wrap around beyond that; the driver chooses the
    // granularity with +prefetchprof-hist-shift. Each memory lane has its own bin memory, so the read-modify-write is
    // safe with several accesses in one cycle.
    val histShift = genWORegInit(Wire(UInt(6.W)), "hist_shift", 6.U)
    val histIndex = genWORegInit(Wire(UInt(histBinsLog2.W)), "hist_index", 0.U)
    hists.zip(dmemFires.zip(dmemAddrs)).foreach { case (h, (fire, addr)) =>
      val bin = (addr.pad(64) >> histShift)(histBinsLog2 - 1, 0)
      when(recording && active(fire)) {
        h.write(bin, h.read(bin) + 1.U)
      }
    }
    // Readout: the driver writes hist_index, then reads the bin (summed over lanes) through hist_data_*.
    val histData = hists.map(_.read(histIndex)).reduce(_ + _)
    attach(histData(cyclesLowWidth - 1, 0), "hist_data_low", ReadOnly)
    attach(histData >> cyclesLowWidth, "hist_data_high", ReadOnly)

    // ---- Prefetch accuracy ---------------------------------------------------------------------------------------
    // Accuracy = useful prefetches / prefetched blocks. A prefetched line's "window" is its lifetime in the L1D: from
    // the fill that the prefetch started until the line is evicted. Per prefetched line we track whether the core
    // demanded it during that window:
    //   fill  (l1d_pf_fill)    -> the line now at (set, fill_way) is prefetched and not yet used
    //   demand(l1d_demand)     -> if a prefetched line with this block address is in the set and unused: first use.
    //                             The first use is timely if the line had arrived (l1d_present), late if the demand
    //                             caught up with the prefetch still in flight.
    //   evict (l1d_evict)      -> the line at (set, fill_way) is gone: used or not decides if the prefetch was useful
    // The table mirrors the L1D: one entry per (set, way), so a line has exactly one slot and entries never collide.
    // A tag per slot lets a demand access find a prefetched line even while its fill is still in flight (a late
    // prefetch still counts as used). `lost` should stay 0; it counts fills landing on a slot that still claims a live
    // line (an eviction was missed, e.g. a coherence probe invalidating a tracked line), and cycles with more than one
    // fill, which the tag memory write port cannot take (the cache accepts one new miss per cycle).
    //
    // Concurrency: with several memory lanes, any number of demands can land in one cycle, together with a fill and/or
    // an eviction. Within a cycle the events are applied in the order demands (first uses) -> evictions -> fills:
    //  - two lanes demanding the same prefetched line in one cycle count one first use (the lower lane);
    //  - an eviction sees a first use made in the same cycle (so the line counts as used);
    //  - a fill onto a slot being evicted in the same cycle takes the slot over (the eviction is accounted first).
    // Requires an L1D that exports the l1d_* events; otherwise all counters read 0.
    val lgBlockBytes = 6 // 64B cache blocks
    val accLabels    = Seq("l1d_pf_fill", "l1d_demand", "l1d_evict", "l1d_req_addr", "l1d_set", "l1d_fill_way", "l1d_present")
    val accLanes     = accLabels.map(l => lanes(l).length).distinct
    val accEnabled   = accLabels.forall(hasLabel)
    require(!accEnabled || accLanes.length == 1, s"PrefetchProf: l1d_* events have different lane counts: ${accLabels.zip(accLabels.map(l => lanes(l).length))}")

    // Second accuracy model, for caches that keep a prefetched line in its miss-handling register until a demand claims
    // it (BOOM): the core exports per-MSHR events instead of per-lane cache events. The lifetime of a prefetched line is
    // then fill -> first demand, or fill -> dropped without one; there are no evictions of unclaimed lines to track.
    val mshrLabels  = Seq("pf_mshr_alloc", "pf_mshr_use_timely", "pf_mshr_use_late", "pf_mshr_drop")
    val mshrEnabled = mshrLabels.forall(hasLabel)
    def laneCount(label: String): UInt = {
      val ls = lanes(label)
      if (ls.isEmpty) 0.U else PopCount(ls.map(e => recording && active(e)))
    }
    val accCounters: Seq[(String, UInt)] = if (mshrEnabled) {
      def counter(inc: UInt): UInt = {
        val c = RegInit(0.U(64.W))
        c := c + inc
        c
      }
      val timelyInc = laneCount("pf_mshr_use_timely")
      val lateInc   = laneCount("pf_mshr_use_late")
      Seq(
        "fills"          -> counter(laneCount("pf_mshr_alloc")),
        "useful"         -> counter(timelyInc +& lateInc),
        // every useful prefetch ends its unclaimed lifetime at its first use; the others end by being dropped
        "evicted_used"   -> counter(timelyInc +& lateInc),
        "evicted_unused" -> counter(laneCount("pf_mshr_drop")),
        "lost"           -> counter(0.U),
        "demands"        -> counter(laneCount("l1d_demand")),
        "timely"         -> counter(timelyInc),
        "late"           -> counter(lateInc),
      )
    } else if (!accEnabled) {
      Seq("fills", "useful", "evicted_used", "evicted_unused", "lost", "demands", "timely", "late").map(_ -> 0.U(64.W))
    } else {
      val n        = accLanes.head
      val setBits  = lanes("l1d_set").head.getWidth
      val wayBits  = lanes("l1d_fill_way").head.getWidth
      val nSets    = 1 << setBits
      val nWays    = 1 << wayBits
      val tagWidth = 64 - lgBlockBytes - setBits

      val valid   = RegInit(VecInit(Seq.fill(nSets * nWays)(false.B)))
      val used    = RegInit(VecInit(Seq.fill(nSets * nWays)(false.B)))
      // one tag memory per way, indexed by set
      val tagMems = Seq.fill(nWays)(Mem(nSets, UInt(tagWidth.W)))
      def slot(set: UInt, way: UInt): UInt = Cat(set, way.pad(wayBits))(setBits + wayBits - 1, 0)

      // per-lane views of the events
      val set      = lanes("l1d_set")
      val fillWay  = lanes("l1d_fill_way")
      val present  = lanes("l1d_present").map(active)
      val tag      = lanes("l1d_req_addr").map(a => (a.pad(64) >> lgBlockBytes) >> setBits)
      val demV     = lanes("l1d_demand").map(d => recording && active(d))
      val fillV    = lanes("l1d_pf_fill").map(f => recording && active(f))
      val evV      = lanes("l1d_evict").map(e => recording && active(e))

      // demands: first use of a prefetched line with this block address, in any way of the set
      val demFirstWay = Seq.tabulate(n) { l =>
        // an earlier lane demanding the same block in this cycle already made the first use
        val dupe = (0 until l).map(m => demV(m) && set(m) === set(l) && tag(m) === tag(l)).foldLeft(false.B)(_ || _)
        VecInit((0 until nWays).map { w =>
          val sl = slot(set(l), w.U)
          demV(l) && !dupe && valid(sl) && !used(sl) && tagMems(w).read(set(l)) === tag(l)
        })
      }
      val demFirst = demFirstWay.map(_.asUInt.orR)

      // evictions of the line at (set, fillWay); a first use of that slot in the same cycle counts as used
      val evSlot = Seq.tabulate(n)(l => slot(set(l), fillWay(l)))
      val evHit  = Seq.tabulate(n)(l => evV(l) && valid(evSlot(l)))
      val evUsed = Seq.tabulate(n) { l =>
        val firstUseHere = (0 until n).flatMap { m =>
          (0 until nWays).map(w => demFirstWay(m)(w) && slot(set(m), w.U) === evSlot(l))
        }.reduce(_ || _)
        used(evSlot(l)) || firstUseHere
      }
      // fill onto a slot still claiming a live line that no eviction accounts for
      val fillLost = Seq.tabulate(n)(l => fillV(l) && valid(evSlot(l)) && !evV(l))

      // slot state: fills take priority over evictions, which take priority over first uses
      for (i <- 0 until nSets * nWays) {
        val iSet  = (i / nWays).U
        val iWay  = (i % nWays).U
        def here(vs: Seq[Bool]) = (0 until n).map(l => vs(l) && set(l) === iSet && fillWay(l) === iWay).reduce(_ || _)
        val demHere = (0 until n).map(l => set(l) === iSet && demFirstWay(l)(i % nWays)).reduce(_ || _)
        when(here(fillV)) {
          valid(i) := true.B
          used(i)  := false.B
        }.elsewhen(here(evV)) {
          valid(i) := false.B
          used(i)  := false.B
        }.elsewhen(demHere) {
          used(i) := true.B
        }
      }

      // tag write: a single port, driven by the first filling lane
      val anyFill = fillV.reduce(_ || _)
      val fillSel = PriorityEncoderOH(VecInit(fillV).asUInt)
      def pick(vals: Seq[UInt]): UInt = Mux1H((0 until n).map(l => fillSel(l) -> vals(l)))
      when(anyFill) {
        for (w <- 0 until nWays) {
          when(pick(fillWay) === w.U) { tagMems(w).write(pick(set), pick(tag)) }
        }
      }
      val multiFill = PopCount(fillV) > 1.U

      def counter(inc: UInt): UInt = {
        val c = RegInit(0.U(64.W))
        c := c + inc
        c
      }
      Seq(
        "fills"          -> counter(PopCount(fillV)),
        "useful"         -> counter(PopCount(demFirst)),
        "evicted_used"   -> counter(PopCount(Seq.tabulate(n)(l => evHit(l) && evUsed(l)))),
        "evicted_unused" -> counter(PopCount(Seq.tabulate(n)(l => evHit(l) && !evUsed(l)))),
        "lost"           -> counter(PopCount(fillLost) +& multiFill.asUInt),
        "demands"        -> counter(PopCount(demV)),
        "timely"         -> counter(PopCount(Seq.tabulate(n)(l => demFirst(l) && present(l)))),
        "late"           -> counter(PopCount(Seq.tabulate(n)(l => demFirst(l) && !present(l)))),
      )
    }
    // ---- Miss and traffic counters ---------------------------------------------------------------------------------
    // Independent of the accuracy tracker; each reads 0 if its event isn't annotated.
    //   demand_misses           demand accesses that missed in the L1D (cacheable only; one count per miss)
    //   demand_misses_secondary the subset that merged into a line already being fetched (e.g., a prefetch in flight)
    //   pf_issues               prefetch requests accepted by the L1D, including ones that hit and fetch nothing
    // Fills (new lines fetched by prefetches) and demands (L1D accesses) come from the accuracy tracker above. With
    // several lanes every active lane counts in the same cycle.
    def eventCounter(label: String): UInt = {
      val c = RegInit(0.U(64.W))
      val inc = PopCount(lanes(label).map(e => recording && active(e)))
      c := c + (if (lanes(label).isEmpty) 0.U else inc)
      c
    }
    val missCounters: Seq[(String, UInt)] = Seq(
      "demand_misses"           -> eventCounter("l1d_demand_miss"),
      "demand_misses_secondary" -> eventCounter("l1d_demand_miss_secondary"),
      "pf_issues"               -> eventCounter("pf_issue"),
    )

    // ---- Memory traffic ------------------------------------------------------------------------------------------
    // Traffic across the L1D boundary (TileLink), summed from events annotated as the bytes moved each cycle, so the bus
    // width does not matter. Cached lines only. Reads 0 for any event that isn't annotated.
    //   mem_in_bytes    lines fetched into the L1D (demand and prefetch), as GrantData beats
    //   mem_out_bytes   dirty lines written back (ReleaseData / ProbeAckData beats)
    //   mem_acquires    Acquire requests sent (a line or only a permission upgrade)
    //   mem_ctrl_msgs   Release / ProbeAck messages without data
    def sumCounter(label: String): UInt = {
      val c  = RegInit(0.U(64.W))
      val ls = lanes(label)
      c := c + (if (ls.isEmpty) 0.U else Mux(recording, ls.map(_.pad(64)).reduce(_ + _), 0.U))
      c
    }
    val trafficLabels = Seq("l1d_in_bytes", "l1d_out_bytes", "l1d_acquire", "l1d_release_ctrl")
    val trafficCounters: Seq[(String, UInt)] = Seq(
      "mem_in_bytes"  -> sumCounter("l1d_in_bytes"),
      "mem_out_bytes" -> sumCounter("l1d_out_bytes"),
      "mem_acquires"  -> sumCounter("l1d_acquire"),
      "mem_ctrl_msgs" -> sumCounter("l1d_release_ctrl"),
    )

    (accCounters ++ missCounters ++ trafficCounters).foreach { case (name, value) =>
      attach(value(cyclesLowWidth - 1, 0), s"acc_${name}_low", ReadOnly)
      attach(value >> cyclesLowWidth, s"acc_${name}_high", ReadOnly)
    }

    // ---- Trace mode (+prefetch_trace) -------------------------------------------------------------------------
    // Streams one 64B record to the driver for every cycle in which a prefetch is issued to the L1D (pf_issue) or
    // starts a line fill (l1d_pf_fill), carrying the target cycle and the core's PC at that moment. Layout, as
    // little-endian 64-bit words:
    //   [0] cycle  [1] pc  [2] pf_issue address  [3] l1d_pf_fill address  [4] flags (bit0 issue valid, bit1 fill valid)
    //   [5..7] reserved
    // Counters keep running in this mode; tracing only adds the stream.
    // Prefetch fills (l1d_pf_fill, l1d_req_addr) are optional: without them (for example a prefetcher that is not a
    // cache-level one) the trace carries issues only.
    val traceLabels    = Seq("pf_issue", "pf_issue_addr", "core_pc")
    val traceAvailable = traceLabels.forall(hasLabel)
    attach(WireInit(traceAvailable.B), "trace_available", ReadOnly)
    val traceEnable    = genWORegInit(Wire(Bool()), "trace_enable", false.B)

    val traceRecValid = Wire(Bool())
    if (traceAvailable) {
      // At most one prefetch issue and one fill per cycle, so one record holds both; if several lanes were active, the
      // lowest one is recorded.
      def first(valids: Seq[UInt], vals: Seq[UInt]): (Bool, UInt) = {
        val v   = VecInit(valids.map(active))
        val oh  = PriorityEncoderOH(v.asUInt)
        (v.asUInt.orR, Mux1H((0 until v.length).map(l => oh(l) -> vals(l).pad(64))))
      }
      val (issueV, issueAddr) = first(lanes("pf_issue"), lanes("pf_issue_addr"))
      val (fillV, fillAddr)   =
        if (hasLabel("l1d_pf_fill") && hasLabel("l1d_req_addr")) first(lanes("l1d_pf_fill"), lanes("l1d_req_addr"))
        else (false.B, 0.U(64.W))
      traceRecValid := (issueV || fillV) && !hPort.hBits.underGlobalReset && triggered
      val words = Seq(
        cycles,
        lanes("core_pc").head,
        issueAddr,
        fillAddr,
        Cat(fillV, issueV),
        0.U,
        0.U,
        0.U,
      ).map(_.pad(64)(63, 0))
      streamEnq.bits := VecInit(words).asUInt
    } else {
      traceRecValid  := false.B
      streamEnq.bits := 0.U
    }
    traceReady       := !(traceEnable && traceRecValid) || streamEnq.ready
    streamEnq.valid  := tFireHelper.fire(traceReady) && traceEnable && traceRecValid

    // ---- Build-time self-check ------------------------------------------------------------------------------------
    // Reports which feature groups have all of their annotated events. A group with a missing event doesn't fail the
    // build (except the required ones, which do): its counters read 0, or +prefetch_trace is rejected at startup.
    def groupStatus(labels: Seq[String]): String = {
      val missing = labels.filterNot(hasLabel)
      if (missing.isEmpty) "ok" else s"MISSING ${missing.mkString(", ")}"
    }
    val missLabels     = Seq("l1d_demand_miss", "l1d_demand_miss_secondary", "pf_issue")
    val usedLabels     = (requiredLabels ++ accLabels ++ mshrLabels ++ traceLabels ++ missLabels ++ trafficLabels).toSet
    println("[PrefetchProf] event groups:")
    println(s"  required (instr_ret, address histogram): ${groupStatus(requiredLabels)}")
    val accStatus =
      if (mshrEnabled) s"ok (per-MSHR model: ${mshrLabels.mkString(", ")})"
      else groupStatus(accLabels) + (if (accEnabled) " (set/way tracker model)" else "")
    println(s"  accuracy / timeliness (counters read 0 if missing): $accStatus")
    println(s"  miss / traffic counters (each reads 0 if its event is missing): ${groupStatus(missLabels)}")
    println(s"  memory traffic counters (each reads 0 if its event is missing): ${groupStatus(trafficLabels)}")
    println(s"  trace, +prefetch_trace (rejected at startup if missing): ${groupStatus(traceLabels)}")
    if (filterTags.nonEmpty) {
      println(s"  filter tags (+prefetchprof-filter=<tag>[,<tag>...], bit order): ${filterTags.mkString(", ")}")
      filterTags.foreach { t =>
        println(s"    $t: ${key.eventMetadata.filter(_.tag == t).map(m => m.vecGroup.getOrElse(m.label)).distinct.mkString(", ")}")
      }
    } else {
      println("  filter tags: none (no annotation carries a tag)")
    }
    println(
      "  annotated events: " +
        key.eventMetadata.map(m => s"${m.label}(${m.width}b)").mkString(", ")
    )
    // Elements of an annotated sequence are used when their group is.
    val unusedLabels = key.eventMetadata.filterNot(m => usedLabels.contains(m.vecGroup.getOrElse(m.label))).map(_.label)
    if (unusedLabels.nonEmpty) {
      println(s"  annotated but not used by the bridge: ${unusedLabels.mkString(", ")}")
    }

    override def genHeader(base: BigInt, memoryRegions: Map[String, BigInt], sb: StringBuilder): Unit = {
      genConstructor(
        base,
        sb,
        "prefetchprof_t",
        "prefetchprof",
        Seq(
          StdVector(
            "prefetchprof_t::Event",
            key.eventMetadata.map { m =>
              CppStruct(
                "prefetchprof_t::Event",
                Seq(
                  "label"       -> CStrLit(m.label),
                  "description" -> CStrLit(m.description),
                  "bit_width"   -> UInt32(m.width),
                ),
              )
            },
          ),
          Verbatim(clockDomainInfo.toC),
          UInt32(histBins),
          UInt32(1 << lgBlockBytes),
          StdVector("std::string", filterTags.map(CStrLit(_))),
          UInt32(toHostStreamIdx),
          UInt32(toHostCPUQueueDepth),
        ),
        hasStreams = true,
      )
    }
    genCRFile()
  }
}
