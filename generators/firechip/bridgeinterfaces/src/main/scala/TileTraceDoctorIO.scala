package firechip.bridgeinterfaces

import chisel3._

class TraceDoctor(val traceWidth : Int) extends Bundle {
  val valid = Bool()
  val bits = Vec(traceWidth, Bool())
}

class TileTraceDoctorIO(val traceWidth: Int) extends Bundle {
  val clock: Clock = Clock()
  val reset: Bool = Bool()
  val data = new TraceDoctor(traceWidth)
}
 