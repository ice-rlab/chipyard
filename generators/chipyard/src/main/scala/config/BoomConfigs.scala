package chipyard

import org.chipsalliance.cde.config.{Config}

// ---------------------
// BOOM V3 Configs
// Performant, stable baseline
// ---------------------

class SmallBoomV3Config extends Config(
  new boom.v3.common.WithNSmallBooms(1) ++                          // small boom config
  new chipyard.config.AbstractConfig)

class MediumBoomV3Config extends Config(
  new boom.v3.common.WithNMediumBooms(1) ++                         // medium boom config
  new chipyard.config.AbstractConfig)

class LargeBoomV3Config extends Config(
  new boom.v3.common.WithNLargeBooms(1) ++                          // large boom config
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class MegaBoomV3Config extends Config(
  new boom.v3.common.WithNMegaBooms(1) ++                           // mega boom config
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class GigaBoomV3Config extends Config(
  new boom.v3.common.WithNGigaBooms(1) ++                           // giga boom config
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class DualSmallBoomV3Config extends Config(
  new boom.v3.common.WithNSmallBooms(2) ++                          // 2 boom cores
  new chipyard.config.AbstractConfig)

class Cloned64MegaBoomV3Config extends Config(
  new boom.v3.common.WithCloneBoomTiles(63, 0) ++
  new boom.v3.common.WithNMegaBooms(1) ++                           // mega boom config
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class LoopbackNICLargeBoomV3Config extends Config(
  new chipyard.harness.WithLoopbackNIC ++                        // drive NIC IOs with loopback
  new icenet.WithIceNIC ++                                       // build a NIC
  new boom.v3.common.WithNLargeBooms(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class MediumBoomV3CosimConfig extends Config(
  new chipyard.harness.WithCospike ++                            // attach spike-cosim
  new chipyard.config.WithTraceIO ++                             // enable the traceio
  new boom.v3.common.WithNMediumBooms(1) ++
  new chipyard.config.AbstractConfig)

class dmiCheckpointingMediumBoomV3Config extends Config(
  new chipyard.config.WithNPMPs(0) ++                            // remove PMPs (reduce non-core arch state)
  new chipyard.harness.WithSerialTLTiedOff ++                    // don't attach anything to serial-tl
  new chipyard.config.WithDMIDTM ++                              // have debug module expose a clocked DMI port
  new boom.v3.common.WithNMediumBooms(1) ++
  new chipyard.config.AbstractConfig)

class dmiMediumBoomV3CosimConfig extends Config(
  new chipyard.harness.WithCospike ++                            // attach spike-cosim
  new chipyard.config.WithTraceIO ++                             // enable the traceio
  new chipyard.harness.WithSerialTLTiedOff ++                    // don't attach anythint to serial-tl
  new chipyard.config.WithDMIDTM ++                              // have debug module expose a clocked DMI port
  new boom.v3.common.WithNMediumBooms(1) ++
  new chipyard.config.AbstractConfig)

class SimBlockDeviceMegaBoomV3Config extends Config(
  new chipyard.harness.WithSimBlockDevice ++                     // drive block-device IOs with SimBlockDevice
  new testchipip.iceblk.WithBlockDevice ++                       // add block-device module to peripherybus
  new boom.v3.common.WithNMegaBooms(1) ++                        // mega boom config
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)


// ---------------------
// BOOM V4 Configs
// Less stable and performant, but with more advanced micro-architecture
// Use for PD exploration
// ---------------------

class SmallBoomV4Config extends Config(
  new boom.v4.common.WithNSmallBooms(1) ++                          // small boom config
  new chipyard.config.AbstractConfig)

class MediumBoomV4Config extends Config(
  new boom.v4.common.WithNMediumBooms(1) ++                         // medium boom config
  new chipyard.config.AbstractConfig)

class LargeBoomV4Config extends Config(
  new boom.v4.common.WithNLargeBooms(1) ++                          // large boom config
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class MegaBoomV4Config extends Config(
  new boom.v4.common.WithNMegaBooms(1) ++                           // mega boom config
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class DualSmallBoomV4Config extends Config(
  new boom.v4.common.WithNSmallBooms(2) ++                          // 2 boom cores
  new chipyard.config.AbstractConfig)

class Cloned64MegaBoomV4Config extends Config(
  new boom.v4.common.WithCloneBoomTiles(63, 0) ++
  new boom.v4.common.WithNMegaBooms(1) ++                           // mega boom config
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class MediumBoomV4CosimConfig extends Config(
  new chipyard.harness.WithCospike ++                            // attach spike-cosim
  new chipyard.config.WithTraceIO ++                             // enable the traceio
  new boom.v4.common.WithNMediumBooms(1) ++
  new chipyard.config.AbstractConfig)

class dmiCheckpointingMediumBoomV4Config extends Config(
  new chipyard.config.WithNPMPs(0) ++                            // remove PMPs (reduce non-core arch state)
  new chipyard.harness.WithSerialTLTiedOff ++                    // don't attach anything to serial-tl
  new chipyard.config.WithDMIDTM ++                              // have debug module expose a clocked DMI port
  new boom.v4.common.WithNMediumBooms(1) ++
  new chipyard.config.AbstractConfig)

class dmiMediumBoomV4CosimConfig extends Config(
  new chipyard.harness.WithCospike ++                            // attach spike-cosim
  new chipyard.config.WithTraceIO ++                             // enable the traceio
  new chipyard.harness.WithSerialTLTiedOff ++                    // don't attach anythint to serial-tl
  new chipyard.config.WithDMIDTM ++                              // have debug module expose a clocked DMI port
  new boom.v4.common.WithNMediumBooms(1) ++
  new chipyard.config.AbstractConfig)

class SimBlockDeviceMegaBoomV4Config extends Config(
  new chipyard.harness.WithSimBlockDevice ++                     // drive block-device IOs with SimBlockDevice
  new testchipip.iceblk.WithBlockDevice ++                       // add block-device module to peripherybus
  new boom.v4.common.WithNMegaBooms(1) ++                        // mega boom config
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)


// ---------------------
// BOOM V3 Performance characterization Configs
// ---------------------

class SmallBoomOneCountersConfig extends Config(
  new boom.v3.common.WithNSmallBooms(1,
    boom.v3.common.SuperscalarCSRMode.NONE, 1) ++
  new chipyard.config.AbstractConfig)

class SmallBoomTwoCountersConfig extends Config(
  new boom.v3.common.WithNSmallBooms(1,
    boom.v3.common.SuperscalarCSRMode.NONE, 2) ++
  new chipyard.config.AbstractConfig)

class SmallBoomScalarCountersConfig extends Config(
  new boom.v3.common.WithNSmallBooms(1,
    boom.v3.common.SuperscalarCSRMode.SCALAR_COUNTERS, 2) ++
  new chipyard.config.AbstractConfig)

class SmallBoomAddWiresConfig extends Config(
  new boom.v3.common.WithNSmallBooms(1,
    boom.v3.common.SuperscalarCSRMode.ADD_WIRES, 2) ++
  new chipyard.config.AbstractConfig)

class SmallBoomDistributedCountersConfig extends Config(
  new boom.v3.common.WithNSmallBooms(1,
    boom.v3.common.SuperscalarCSRMode.DISTRIBUTED_COUNTERS, 2) ++
  new chipyard.config.AbstractConfig)

class MediumBoomScalarCountersConfig extends Config(
  new boom.v3.common.WithNMediumBooms(1,
    boom.v3.common.SuperscalarCSRMode.SCALAR_COUNTERS, 4) ++
  new chipyard.config.AbstractConfig)

class MediumBoomAddWiresConfig extends Config(
  new boom.v3.common.WithNMediumBooms(1,
    boom.v3.common.SuperscalarCSRMode.ADD_WIRES, 2) ++
  new chipyard.config.AbstractConfig)

class MediumBoomDistributedCountersConfig extends Config(
  new boom.v3.common.WithNMediumBooms(1,
    boom.v3.common.SuperscalarCSRMode.DISTRIBUTED_COUNTERS, 2) ++
  new chipyard.config.AbstractConfig)

class LargeBoomScalarCountersConfig extends Config(
  new boom.v3.common.WithNLargeBooms(1,
    boom.v3.common.SuperscalarCSRMode.SCALAR_COUNTERS, 6) ++
  new chipyard.config.AbstractConfig)

class LargeBoomAddWiresConfig extends Config(
  new boom.v3.common.WithNLargeBooms(1,
    boom.v3.common.SuperscalarCSRMode.ADD_WIRES, 2) ++
  new chipyard.config.AbstractConfig)

class LargeBoomDistributedCountersConfig extends Config(
  new boom.v3.common.WithNLargeBooms(1,
    boom.v3.common.SuperscalarCSRMode.DISTRIBUTED_COUNTERS, 2) ++
  new chipyard.config.AbstractConfig)

class MegaBoomNPerf1Config extends Config(
  new boom.v3.common.WithNMegaBooms(1,
    boom.v3.common.SuperscalarCSRMode.NONE, 1) ++
  new chipyard.config.AbstractConfig)

class MegaBoomNPerf2Config extends Config(
  new boom.v3.common.WithNMegaBooms(1,
    boom.v3.common.SuperscalarCSRMode.NONE, 2) ++
  new chipyard.config.AbstractConfig)

class MegaBoomScalarCountersConfig extends Config(
  new boom.v3.common.WithNMegaBooms(1,
    boom.v3.common.SuperscalarCSRMode.SCALAR_COUNTERS, 8) ++
  new chipyard.config.AbstractConfig)

class MegaBoomAddWiresConfig extends Config(
  new boom.v3.common.WithNMegaBooms(1,
    boom.v3.common.SuperscalarCSRMode.ADD_WIRES, 2) ++
  new chipyard.config.AbstractConfig)

class MegaBoomDistributedCountersConfig extends Config(
  new boom.v3.common.WithNMegaBooms(1,
    boom.v3.common.SuperscalarCSRMode.DISTRIBUTED_COUNTERS, 2) ++
  new chipyard.config.AbstractConfig)

class GigaBoomScalarCountersConfig extends Config(
  new boom.v3.common.WithNGigaBooms(1,
    boom.v3.common.SuperscalarCSRMode.SCALAR_COUNTERS, 10) ++
  new chipyard.config.AbstractConfig)

class GigaBoomAddWiresConfig extends Config(
  new boom.v3.common.WithNGigaBooms(1,
    boom.v3.common.SuperscalarCSRMode.ADD_WIRES, 2) ++
  new chipyard.config.AbstractConfig)

class GigaBoomDistributedCountersConfig extends Config(
  new boom.v3.common.WithNGigaBooms(1,
    boom.v3.common.SuperscalarCSRMode.DISTRIBUTED_COUNTERS, 2) ++
  new chipyard.config.AbstractConfig)




// // From Protoacc paper configs:
// class ProtoMegaBoomBaseConfigNoAccel extends Config(
  // new chipyard.harness.WithUARTAdapter ++
  // new chipyard.harness.WithTieOffInterrupts ++
  // new chipyard.harness.WithBlackBoxSimMem ++
  // new chipyard.harness.WithTiedOffDebug ++
  // new chipyard.harness.WithSimSerial ++
  // new testchipip.WithTSI ++
  // new chipyard.config.WithBootROM ++
  // new chipyard.config.WithUART ++
  // new chipyard.config.WithL2TLBs(1024) ++
  // new freechips.rocketchip.subsystem.WithNoMMIOPort ++
  // new freechips.rocketchip.subsystem.WithNoSlavePort ++
  // new freechips.rocketchip.subsystem.WithInclusiveCache(nBanks=8, nWays=16, capacityKB=2048) ++
  // new WithExtMemIdBits(7) ++
  // new freechips.rocketchip.subsystem.WithNMemoryChannels(4) ++
  // new freechips.rocketchip.subsystem.WithNExtTopInterrupts(0) ++
  // new boom.common.WithBoomCommitLogPrintf ++
  // new boom.common.WithMegaBooms ++                              // mega boom config
  // new boom.common.WithNBoomCores(1) ++
  // new freechips.rocketchip.subsystem.WithCoherentBusTopology ++
  // new freechips.rocketchip.system.BaseConfig)

// class ProtoSerMegaBoomConfig extends Config(
//   new protoacc.WithProtoAccelSerOnly ++
//   new ProtoMegaBoomBaseConfigNoAccel)
