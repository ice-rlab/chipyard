
package firechip.bridgestubs

import chisel3._

import org.chipsalliance.cde.config.{Config, Field, Parameters}

import midas.targetutils.TriggerSink

import firechip.bridgeinterfaces._



class TraceDoctorDUTIO(val traceWidth: Int) extends Bundle {
  val triggerSink = Output(Bool())
  val data = Input(new TraceDoctor(traceWidth))
}

class TraceDoctorDUT(implicit val p: Parameters) extends Module {
  val io = IO(new TraceDoctorDUTIO(4))


  //TODO: Testing
  // val traceDoctorBridge = TraceDoctorBridge(4)
  // traceDoctorBridge.io.t
  // traceDoctorBridge.io.tiletrace.data := io.data
  // traceDoctorBridge.io.tiletrace.trace.time := 0.U // this test ignores this
  // TriggerSink(io.triggerSink)
}

class TraceDoctorModule(implicit p: Parameters) extends firesim.lib.testutils.PeekPokeHarness(() => new TraceDoctorDUT)