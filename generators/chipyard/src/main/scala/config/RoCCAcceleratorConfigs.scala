package chipyard

import org.chipsalliance.cde.config.{Config}

// ------------------------------
// Configs with RoCC Accelerators
// ------------------------------

// DOC include start: GemminiRocketConfig
class GemminiRocketConfig extends Config(
  new gemmini.DefaultGemminiConfig ++                            // use Gemmini systolic array GEMM accelerator
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)
// DOC include end: GemminiRocketConfig

class FPGemminiRocketConfig extends Config(
  new gemmini.GemminiFP32DefaultConfig ++                         // use FP32Gemmini systolic array GEMM accelerator
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class LeanGemminiRocketConfig extends Config(
  new gemmini.LeanGemminiConfig ++                                 // use Lean Gemmini systolic array GEMM accelerator
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class LeanGemminiPrintfRocketConfig extends Config(
  new gemmini.LeanGemminiPrintfConfig ++                                 // use Lean Gemmini systolic array GEMM accelerator
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class MempressRocketConfig extends Config(
  new mempress.WithMemPress ++                                    // use Mempress (memory traffic generation) accelerator
  new chipyard.config.WithExtMemIdBits(7) ++                      // use 7 bits for tl like request id
  new chipyard.config.WithSystemBusWidth(128) ++
  new freechips.rocketchip.subsystem.WithNBanks(8) ++
  new freechips.rocketchip.subsystem.WithInclusiveCache(nWays=16, capacityKB=2048) ++
  new freechips.rocketchip.subsystem.WithNMemoryChannels(4) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class AES256ECBRocketConfig extends Config(
  new aes.WithAES256ECBAccel ++                                   // use Caliptra AES 256 ECB accelerator
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(256) ++
  new chipyard.config.AbstractConfig)

class ReRoCCTestConfig extends Config(
  new rerocc.WithReRoCC ++
  new chipyard.config.WithCharacterCountRoCC ++                // rerocc tile4 is charcnt
  new chipyard.config.WithAccumulatorRoCC ++                   // rerocc tile3 is accum
  new chipyard.config.WithAccumulatorRoCC ++                   // rerocc tile2 is accum
  new chipyard.config.WithAccumulatorRoCC ++                   // rerocc tile1 is accum
  new chipyard.config.WithAccumulatorRoCC ++                   // rerocc tile0 is accum
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)

class ReRoCCManyGemminiConfig extends Config(
  new rerocc.WithReRoCC ++
  new gemmini.LeanGemminiConfig ++                              // rerocc tile3 is gemmini
  new gemmini.LeanGemminiConfig ++                              // rerocc tile2 is gemmini
  new gemmini.LeanGemminiConfig ++                              // rerocc tile1 is gemmini
  new gemmini.LeanGemminiConfig ++                              // rerocc tile0 is gemmini
  new freechips.rocketchip.rocket.WithNHugeCores(4) ++           // 4 rocket cores
  new chipyard.config.AbstractConfig)

class ZstdCompressorRocketConfig extends Config(
  new compressacc.WithZstdCompressor ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)


// class ZstdCompressorRocketConfig extends Config(
//   new compressacc.WithSnappyCompleteFireSim ++
//   new freechips.rocketchip.rocket.WithNHugeCores(1) ++
//   new chipyard.config.AbstractConfig)


class ProtoAccRocketConfig extends Config(
  new protoacc.WithProtoAccelSerOnly ++ 
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++ 
  new chipyard.config.AbstractConfig
)







//////////////////////////////////////
// THIS IS FROM CDPU PAPER ARTIFACT //
//////////////////////////////////////

// class WithExtMemIdBits(n: Int) extends Config((site, here, up) => {
//   case ExtMem => up(ExtMem, site).map(x => x.copy(master = x.master.copy(idBits = n)))
// })

// class CompressAccBaseConfig extends Config(
//   new freechips.rocketchip.subsystem.WithInclusiveCache(nWays=16, capacityKB=2048) ++
//   new freechips.rocketchip.subsystem.WithNBanks(8) ++
//   new WithExtMemIdBits(7) ++
//   new freechips.rocketchip.subsystem.WithNMemoryChannels(4) ++
//   new Config ((site, here, up) => {
//     case SystemBusKey => up(SystemBusKey, site).copy(beatBytes = 32)
//   }) ++
//   new RocketConfig)

// class CompressAccBaseConfig16MBL2 extends Config(
//   new freechips.rocketchip.subsystem.WithInclusiveCache(nWays=16, capacityKB=16*1024) ++
//   new CompressAccBaseConfig)

// class CompressAccBaseConfig16MBL2And8MemChan extends Config(
//   new freechips.rocketchip.subsystem.WithNMemoryChannels(8) ++
//   new CompressAccBaseConfig16MBL2)

// class SnappyBothConfig16MBL2And8MemChanRoCC extends Config(
//   new compressacc.AcceleratorPlacementRoCC ++
//   new compressacc.WithSnappyCompleteFireSim ++
//   new CompressAccBaseConfig16MBL2And8MemChan)

// class SnappyBothConfig16MBL2And8MemChanChiplet extends Config(
//   new compressacc.AcceleratorPlacementChiplet ++
//   new compressacc.WithSnappyCompleteFireSim ++
//   new CompressAccBaseConfig16MBL2And8MemChan)

// class SnappyBothConfig16MBL2And8MemChanPCIeLocalCache extends Config(
//   new compressacc.AcceleratorPlacementPCIeLocalCache ++
//   new compressacc.WithSnappyCompleteFireSim ++
//   new CompressAccBaseConfig16MBL2And8MemChan)

// class SnappyBothConfig16MBL2And8MemChanPCIeNoCache extends Config(
//   new compressacc.AcceleratorPlacementPCIeNoCache ++
//   new compressacc.WithSnappyCompleteFireSim ++
//   new CompressAccBaseConfig16MBL2And8MemChan)

// class SnappyDecompressorConfig16MBL2And8MemChanPCIeNoCache extends Config(
//   new compressacc.AcceleratorPlacementPCIeNoCache ++
//   new compressacc.WithSnappyDecompressor ++
//   new CompressAccBaseConfig16MBL2And8MemChan)

//////////////////////////////////////
// END OF ARTIFACT                  //
//////////////////////////////////////