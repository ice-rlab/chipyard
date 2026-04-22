//See LICENSE for license details
package firechip.bridgestubs

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.{Config, Field, Parameters}
import freechips.rocketchip.util._

import midas.targetutils.{TriggerSource, TriggerSink}
import firesim.lib.bridgeutils._

import firechip.bridgeinterfaces._

class TraceDoctorBridge(val key: TraceDoctorKey) extends BlackBox
    with Bridge[HostPortIO[TraceDoctorTargetIO]] {
  val moduleName = "firechip.goldengateimplementations.TraceDoctorBridgeModule"
  val io = IO(new TraceDoctorTargetIO(key.traceWidth, key.eventMetadata, key.triggerName, key.resetPortName))
  val bridgeIO = HostPort(io)
  val constructorArg = Some(key)
  generateAnnotations()
}
