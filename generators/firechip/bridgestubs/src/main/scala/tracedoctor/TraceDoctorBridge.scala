//See LICENSE for license details
package firechip.bridgestubs

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.{Config, Field, Parameters}
import freechips.rocketchip.util._

import midas.targetutils.{TriggerSource, TriggerSink}
import firesim.lib.bridgeutils._

import firechip.bridgeinterfaces._

class TraceDoctorBridge(val traceWidth: Int) extends BlackBox
    with Bridge[HostPortIO[TraceDoctorTargetIO]] {
  val moduleName = "firechip.goldengateimplementations.TraceDoctorBridgeModule"
  val io = IO(new TraceDoctorTargetIO(traceWidth))
  val bridgeIO = HostPort(io)
  val constructorArg = Some(TraceDoctorKey(traceWidth))
  generateAnnotations()
}

object TraceDoctorBridge {
  //def apply(tracedoctor: testchipip.TileTraceDoctorIO)(implicit p:Parameters): TraceDoctorBridge = {
  //  val ep = Module(new TraceDoctorBridge(tracedoctor.traceWidth))
  //  ep.io.clock := Module.clock
  //  ep.io.reset := Module.reset
  //  ep.io.trace := tracedoctor
  //  ep.io.tracerVTrigger := tracedoctor.tracerVTrigger
  //  ep
  //}
  //
  //def apply(traceWidth: Int)(implicit p: Parameters): TraceDoctorBridge = {
  //  TraceDoctorBridge(traceWidth)
  //}

  def apply(tracedoctor: testchipip.TileTraceDoctorIO)(implicit p: Parameters): TraceDoctorBridge = {
    val ep = withClockAndReset(tracedoctor.clock, tracedoctor.reset) {
      val mod = Module(new TraceDoctorBridge(tracedoctor.traceWidth))
      mod.io.clock := Module.clock
      mod.io.reset := Module.reset
      mod
    }
    //ep.io.trace <> ConvertTraceDoctorIO(tracedoctor)
    //ep.io.tracerVTrigger := tracedoctor.tracerVTrigger
    //ep.io.trace <> ConvertTraceDoctorIO(tracedoctor)
    ep.io.trace.bits := tracedoctor.bits
    ep.io.trace.valid := tracedoctor.valid
    ep.io.trace.tracerVTrigger := tracedoctor.tracerVTrigger
    ep.io.tracerVTrigger := tracedoctor.tracerVTrigger
    ep
  }
}

//class TraceDoctorTargetIO(val traceWidth : Int) extends Bundle {
//  val clock = Clock()
//  val reset = Bool()
//  val trace = Input(new TraceDoctorBundle(traceWidth))
//  val tracerVTrigger = Bool()
//}
