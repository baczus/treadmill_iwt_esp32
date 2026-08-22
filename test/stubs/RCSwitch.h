#pragma once
#include <vector>
#include <cstdint>

struct RFEvent {
  unsigned long code;
  int bits;
  int pulseLength;
};

extern std::vector<RFEvent> rf_log;   // defined in fake.cpp

class RCSwitch {
public:
  void enableTransmit(int) {}
  void setProtocol(int) {}
  void setRepeatTransmit(int) {}
  void setPulseLength(int pl) { curPulse = pl; }
  void send(unsigned long code, int bits) {
    rf_log.push_back({code, bits, curPulse});
  }
private:
  int curPulse = 425;
};
