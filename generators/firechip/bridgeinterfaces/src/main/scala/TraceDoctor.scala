// See LICENSE for license details.

package firechip.bridgeinterfaces

import chisel3._

class TraceDoctorBundle(val traceWidth: Int) extends Bundle {
  val valid = Bool()
  val bits = Vec(traceWidth, Bool())
  val tracerVTrigger = Bool()
}

//class TileTraceDoctorIO(val traceWidth: Int) extends Bundle {
//  val clock = Clock()
//  val reset = Bool()
//  val valid = Bool()
//  val bits = Vec(traceWidth, Bool())
//  val tracerVTrigger = Bool()
//  //val trace = new TraceDoctorBundle(traceWidth)
//}

case class TraceDoctorKey(traceWidth: Int)

class TraceDoctorTargetIO(val traceWidth : Int) extends Bundle {
  val clock = Input(Clock())
  val reset = Input(Bool())
  val trace = Input(new TraceDoctorBundle(traceWidth))
  val tracerVTrigger = Input(Bool())
}
