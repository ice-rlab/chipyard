//See LICENSE for license details
package firechip.bridgestubs
 
import chisel3._
import chisel3.util._
import org.chipsalliance.cde.config.Parameters

import freechips.rocketchip.util._


import midas.targetutils.TriggerSink
import midas.targetutils.TriggerSource

import firesim.lib.bridgeutils._
import firechip.bridgeinterfaces._


class TraceDoctorBridge(val traceWidth: Int) extends BlackBox with Bridge[HostPortIO[TraceDoctorBridgeTargetIO]] {
    require(traceWidth > 0, "TracerVBridge: number of instructions must be larger than 0")
    val moduleName = "firechip.goldengateimplementations.TraceDoctorBridgeModule"
    val io = IO(new TraceDoctorBridgeTargetIO(traceWidth))
    val bridgeIO = HostPort(io)
    val constructorArg = Some(TraceDoctorKey(traceWidth))
    generateAnnotations()


    // def defnameSuffix = s"_${widths}Wide_" + widths.toString.replaceAll("[(),]", "_")
}

object TraceDoctorBridge {
  def apply(traceWidth: Int)(implicit p:Parameters): TraceDoctorBridge = {
    val tracedoctor = Module(new TraceDoctorBridge(traceWidth))
    // withClockAndReset(tracedoctor.clock, tracedoctor.reset) { TriggerSink(ep.io.tracevTrigger, noSourceDefault = false.B) }
    // tracedoctor.generateTriggerAnnotations()
    tracedoctor.io.tiletrace.clock := Module.clock
    tracedoctor.io.tiletrace.reset := Module.reset
    // tracedoctor.io.tiletrace := tracedoctor
    tracedoctor
  }
  def apply(tracedData: testchipip.cosim.TileTraceDoctorIO)(implicit p: Parameters): TraceDoctorBridge = {
  val tracedoctor = withClockAndReset(tracedData.clock, tracedData.reset) {
      TraceDoctorBridge(tracedData.getTraceWidth())
    }
  tracedoctor.io.tiletrace <> ConvertTileTraceDoctorIO(tracedData)
  tracedoctor
  }
}