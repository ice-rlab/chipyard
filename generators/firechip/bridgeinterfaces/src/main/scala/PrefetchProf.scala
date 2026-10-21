// See LICENSE for license details.

package firechip.bridgeinterfaces

import chisel3._

/** Target-side description of one annotated prefetcher event (e.g., prefetch issued, prefetch hit, demand miss).
  *
  * @param portName
  *   the name of the top-level port exposed to the bridge by the PrefetchProf transform
  * @param label
  *   the user provided label, see midas.targetutils.PrefetchProfTarget
  * @param description
  *   a human-friendly description of the event
  * @param width
  *   the bitwidth of the event
  * @param vecGroup
  *   for an element of an annotated sequence, the name of the sequence
  * @param vecIndex
  *   for an element of an annotated sequence, its position
  * @param tag
  *   optional filter tag, empty if untagged
  */
case class PrefetchProfEventMetadata(
  portName:    String,
  label:       String,
  description: String,
  width:       Int,
  // Set for elements of an annotated sequence (PrefetchProfVecTarget): the group name and the position in it.
  vecGroup:    Option[String] = None,
  vecIndex:    Int            = 0,
  // Optional filter tag: the bridge only records the event while the tag is selected with +prefetchprof-filter.
  tag:         String         = "",
)

object PrefetchProfEventMetadata {
  val localCycleCount =
    PrefetchProfEventMetadata("N/A", "local_cycle", "Clock cycles elapsed in the local domain.", 1)
}

case class PrefetchProfKey(
  eventMetadata: Seq[PrefetchProfEventMetadata],
  triggerName:   String,
  resetPortName: String,
)

/** The target-side IO of the bridge: one input per annotated event, plus a trigger and a global reset indicator. */
class PrefetchProfTargetIO(
  eventMetadata: Seq[PrefetchProfEventMetadata],
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
