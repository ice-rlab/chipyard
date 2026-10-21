// See LICENSE for license details
package firechip.bridgestubs

import chisel3._

import firesim.lib.bridgeutils._

import firechip.bridgeinterfaces._

/** Target-side stub for the PrefetchProf bridge.
  *
  * Normally the bridge is not instantiated by hand: the PrefetchProfTransform creates it from
  * [[midas.targetutils.PrefetchProfTarget]] annotations. This stub exists so the bridge can also be instantiated
  * directly in a target design (e.g., for unit tests).
  */
class PrefetchProfBridge(val key: PrefetchProfKey)
    extends BlackBox
    with Bridge[HostPortIO[PrefetchProfTargetIO]] {
  val moduleName     = "firechip.goldengateimplementations.PrefetchProfBridgeModule"
  val io             = IO(new PrefetchProfTargetIO(key.eventMetadata, key.triggerName, key.resetPortName))
  val bridgeIO       = HostPort(io)
  val constructorArg = Some(key)
  generateAnnotations()
}
