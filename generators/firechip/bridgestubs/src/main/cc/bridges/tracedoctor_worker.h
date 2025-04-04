#ifndef __TRACEDOCTOR_WORKER_H_
#define __TRACEDOCTOR_WORKER_H_

#include <vector>
#include <tuple>
#include <string>
#include <map>
#include <cassert>
#include <cstdint>

struct traceInfo {
  unsigned int tracerId;
  unsigned int tokenBits;
  unsigned int tokenBytes;
  unsigned int traceBits;
  unsigned int traceBytes;
};

void strReplaceAll(std::string &str, std::string const from, std::string const);

std::vector<std::string> strSplit(std::string const, std::string const);

enum fileRegisterFields {freg_name = 0, freg_descriptor = 1, freg_file = 2};

#define TDWORKER_NO_FILES   0
#define TDWORKER_ANY_FILES -1

class tracedoctor_worker {
protected:
  std::string const name;
  std::string const tracerName;
  struct traceInfo const info;
  unsigned int compressionThreads = 1;
  unsigned int compressionLevel = 1;
  std::vector<std::tuple<std::string, FILE *, bool>> fileRegister;

  FILE * openFile(std::string const filename);
  void closeFile(FILE * const);
  void closeFiles(void);

public:
  tracedoctor_worker(std::string const name, std::vector<std::string> const args, struct traceInfo const info, int const requiredFiles = TDWORKER_NO_FILES);
  virtual void tick(char const * const data, unsigned int tokens);
  ~tracedoctor_worker();
};

class tracedoctor_filedumper : public tracedoctor_worker {
private:
  uint64_t byteCount;
  bool raw;
public:
  tracedoctor_filedumper(std::vector<std::string> const args, struct traceInfo const info);
  ~tracedoctor_filedumper();
  void tick(char const * const data, unsigned int tokens);
};

#endif // __TRACEDOCTOR_WORKER_H_