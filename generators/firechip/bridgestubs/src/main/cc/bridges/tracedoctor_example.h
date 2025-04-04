#ifndef __TRACEDOCTOR_EXAMPLE_H_
#define __TRACEDOCTOR_EXAMPLE_H_

#include "tracedoctor_worker.h"

class tracedoctor_tracerv : public tracedoctor_worker {
private:
  struct traceLayout {
    uint64_t timestamp;
    uint64_t valids;
    uint64_t instr0;
    uint64_t instr1;
    uint64_t instr2;
    uint64_t instr3;
    uint64_t instr4;
    uint64_t instr5;
  };

  bool binary;
public:
  tracedoctor_tracerv(std::vector<std::string> const args, struct traceInfo const info);
  ~tracedoctor_tracerv();
  void tick(char const * const data, unsigned int tokens);
};

#endif // __TRACEDOCTOR_EXAMPLE_H_