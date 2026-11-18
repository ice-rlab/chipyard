// See LICENSE for license details.

package firechip.bridgeinterfaces

import chisel3._

case class TraceDoctorEventMetadata(
  portName:    String,
  label:       String,
  description: String,
  width:       Int,
)

object TraceDoctorEventMetadata {
  val localCycleCount =
    TraceDoctorEventMetadata("N/A", "local_cycle", "Clock cycles elapsed in the local domain.", 1)
}

case class TraceDoctorKey(traceWidth: Int, eventMetadata: Seq[TraceDoctorEventMetadata], triggerName: String, resetPortName: String)

class TraceDoctorTargetIO(
  val traceWidth: Int,
  eventMetadata: Seq[TraceDoctorEventMetadata],
  triggerName:   String,
  resetPortName: String,
) extends Record {
  val triggerEnable    = Input(Bool())
  val underGlobalReset = Input(Bool())
  val events           = eventMetadata.map(e => e.portName -> Input(UInt(e.width.W)))
  val elements         = collection.immutable.ListMap(
    ((triggerName, triggerEnable) +:
      (resetPortName, underGlobalReset) +:
      events): _*
  )
}
