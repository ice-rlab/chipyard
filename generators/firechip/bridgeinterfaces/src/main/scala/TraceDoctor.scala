// See LICENSE for license details.

package firechip.bridgeinterfaces

import chisel3._

case class TraceDoctorKey(traceWidth: Int)

class TraceDoctorBridgeTargetIO(val traceWidth: Int) extends Bundle {
  val tiletrace = Input(new TileTraceDoctorIO(traceWidth))
  val triggerCredit = Output(Bool())
  val triggerDebit  = Output(Bool())
}
