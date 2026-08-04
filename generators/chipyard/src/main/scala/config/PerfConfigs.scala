package chipyard

import org.chipsalliance.cde.config.{Config}

class RocketPerfConfig extends Config(
  new chipyard.config.WithNPerfCounters(29) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++ 
  new chipyard.config.AbstractConfig)

class SmallBoomV3CTRPerfConfig extends Config(
  new boom.v3.common.WithNBoomCTR(32) ++
  new chipyard.config.WithNPerfCounters(29) ++
  new boom.v3.common.WithNSmallBooms(1) ++
  new chipyard.config.AbstractConfig)

class SmallBoomV3CTR256PerfConfig extends Config(
  new boom.v3.common.WithNBoomCTR(256) ++
  new chipyard.config.WithNPerfCounters(29) ++
  new boom.v3.common.WithNSmallBooms(1) ++
  new chipyard.config.AbstractConfig)

class SmallBoomV3CTR16PerfConfig extends Config(
  new boom.v3.common.WithNBoomCTR(16) ++
  new chipyard.config.WithNPerfCounters(29) ++
  new boom.v3.common.WithNSmallBooms(1) ++
  new chipyard.config.AbstractConfig)

class SmallBoomV3CTR16MAR16PerfConfig extends Config(
  new boom.v3.common.WithNBoomCTR(16) ++
  new boom.v3.common.WithNBoomMAR(16) ++
  new chipyard.config.WithNPerfCounters(29) ++
  new boom.v3.common.WithNSmallBooms(1) ++
  new chipyard.config.AbstractConfig)

class LargeBoomV3CTR16PerfConfig extends Config(
  new boom.v3.common.WithNBoomCTR(16) ++
  new chipyard.config.WithNPerfCounters(29) ++
  new boom.v3.common.WithNLargeBooms(1) ++
  new chipyard.config.AbstractConfig)

class LargeBoomV3CTR16AR16PerfConfig extends Config(
  new boom.v3.common.WithNBoomCTR(16) ++
  new boom.v3.common.WithNBoomMAR(16) ++
  new chipyard.config.WithNPerfCounters(29) ++
  new boom.v3.common.WithNLargeBooms(1) ++
  new chipyard.config.AbstractConfig)

class LargeBoomLLVCV3CTR16AR16PerfConfig extends Config(
  new freechips.rocketchip.subsystem.WithInclusiveCache ++
  new boom.v3.common.WithNBoomCTR(16) ++
  new boom.v3.common.WithNBoomMAR(16) ++
  new chipyard.config.WithNPerfCounters(29) ++
  new boom.v3.common.WithNLargeBooms(1) ++
  new chipyard.config.AbstractConfig)

class LargeBoomLLVCV3CTR32AR32PerfConfig extends Config(
  new freechips.rocketchip.subsystem.WithInclusiveCache ++
  new boom.v3.common.WithNBoomCTR(32) ++
  new boom.v3.common.WithNBoomMAR(32) ++
  new chipyard.config.WithNPerfCounters(29) ++
  new boom.v3.common.WithNLargeBooms(1) ++
  new chipyard.config.AbstractConfig)
  