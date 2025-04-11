// See LICENSE for license details

package firechip.goldengateimplementations

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.Parameters
import freechips.rocketchip.util._

import midas.widgets._
import firesim.lib.bridgeutils._

import firechip.bridgeinterfaces._

class TraceDoctorBridgeModule(key: TraceDoctorKey)(implicit p: Parameters)
    extends BridgeModule[HostPortIO[TraceDoctorBridgeTargetIO]]()(p)
    with StreamToHostCPU {

  val toHostCPUQueueDepth = TokenQueueConsts.TOKEN_QUEUE_DEPTH

  lazy val module = new BridgeModuleImp(this) {
    val io = IO(new WidgetIO)
    val hPort: HostPortIO[TraceDoctorBridgeTargetIO] = IO(
      HostPort(new TraceDoctorBridgeTargetIO(key.traceWidth))
    )

    // Set after trigger-dependent memory-mapped registers have been set, to
    // prevent spurious credits
    val initDone = genWORegInit(Wire(Bool()), "initDone", false.B)
    val traceEnable = genWORegInit(Wire(Bool()), "traceEnable", false.B)

    // Trigger Selector
    val triggerSelector = RegInit(0.U((p(CtrlNastiKey).dataBits).W))
    attach(triggerSelector, "triggerSelector", WriteOnly)

    // Mask off ready samples when under reset
    val trace = hPort.hBits.tiletrace.data
    val traceValid = trace.valid && !hPort.hBits.tiletrace.reset
    //  val triggerTraceV = hPort.hBits.tracevTrigger
    val triggerTraceV = false.B

    // Connect trigger
    val trigger = MuxLookup(
      triggerSelector,
      false.B,
      Seq(
        0.U -> true.B,
        1.U -> triggerTraceV
      )
    )

    val traceOut = initDone && traceEnable && traceValid && trigger

    // Width of the trace vector
    val traceWidth = trace.bits.getWidth
    // Width of one token as defined by the DMA
    val discreteDmaWidth = TokenQueueConsts.BIG_TOKEN_WIDTH
    // How many tokens we need to trace out the bit vector, at least one for DMA sanity
    val tokensPerTrace =
      math.max((traceWidth + discreteDmaWidth - 1) / discreteDmaWidth, 1)

    // Bridge DMA Parameters
    lazy val dmaSize = BigInt(
      (discreteDmaWidth / 8) * TokenQueueConsts.TOKEN_QUEUE_DEPTH
    )

    // TODO: the commented out changes below show how multi-token transfers would work
    // However they show a bad performance for yet unknown reasons in terms of FPGA synth
    // timings -- verilator shows expected results
    // for now we limit us to 512 bits with an assert.

    assert(tokensPerTrace == 1)

    // State machine that controls which token we are sending and whether we are finished
    // val tokenCounter = new Counter(tokensPerTrace)
    // val readyNextTrace = WireInit(true.B)
    // when (streamEnq.fire()) {
    //  readyNextTrace := tokenCounter.inc()
    // }

    println("TraceDoctorBridgeModule")
    println(s"    traceWidth      ${traceWidth}")
    println(s"    dmaTokenWidth   ${discreteDmaWidth}")
    println(s"    requiredTokens  {")
    for (i <- 0 until tokensPerTrace) {
      val from = ((i + 1) * discreteDmaWidth) - 1
      val to = i * discreteDmaWidth
      println(s"        ${i} -> traceBits(${from}, ${to})")
    }
    println("    }")
    println("")

    // val paddedTrace = trace.bits.asUInt().pad(tokensPerTrace * discreteDmaWidth)
    // val paddedTraceSeq = for (i <- 0 until tokensPerTrace) yield {
    //   i.U -> paddedTrace(((i + 1) * discreteDmaWidth) - 1, i * discreteDmaWidth)
    // }

    // streamEnq.valid := hPort.toHost.hValid && traceOut
    // streamEnq.bits := MuxLookup(tokenCounter.value , 0.U, paddedTraceSeq)

    // hPort.toHost.hReady := initDone && streamEnq.ready && readyNextTrace

    val maybeFire = true.B
    val commonPredicates =
      Seq(hPort.toHost.hValid, hPort.fromHost.hReady, streamEnq.ready, initDone)
    val do_fire_helper = DecoupledHelper((maybeFire +: commonPredicates): _*)
    val triggerReg = RegEnable(trigger, false.B, do_fire_helper.fire())
    hPort.hBits.triggerDebit := !trigger && triggerReg
    hPort.hBits.triggerCredit := trigger && !triggerReg

    streamEnq.valid := hPort.toHost.hValid && traceOut
    streamEnq.bits := trace.bits.asUInt.pad(discreteDmaWidth)

    hPort.toHost.hReady := initDone && streamEnq.ready
    hPort.fromHost.hValid := true.B

    genCRFile()

    override def genHeader(
        base: BigInt,
        memoryRegions: Map[String, BigInt],
        sb: StringBuilder
    ): Unit = {
      genConstructor(
        base,
        sb,
        "tracedoctor_t",
        "tracedoctor",
        Seq(
          UInt32(toHostStreamIdx),
          UInt32(toHostCPUQueueDepth),
          UInt32(trace.traceWidth),
          UInt32(discreteDmaWidth),
          Verbatim(clockDomainInfo.toC)
        ),
        hasStreams = true,
      )
    }
  }
}
