#pragma once

#include <libserial/SerialPort.h>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class LptListener final : public ILptListener {
public:
  LptListener(const ISettingsRepository& settingsRepository, ILptStore& lptStore, SharedLogger log);
  ~LptListener() override;

  std::vector<std::uint8_t> captureTransmission(const std::atomic_bool& shouldKeepRunning) override;
  void test() override;

private:
  void ensureSerialPortOpen(const SerialSettings& serialSettings);
  void closeSerialPortIfOpen();

  const ISettingsRepository& m_SettingsRepository;
  ILptStore& m_LptStore;
  SharedLogger m_Log;
  LibSerial::SerialPort m_SerialPort;
  bool m_IsSerialPortOpen{false};
  std::string m_OpenDevicePath;
};

} // namespace brake_tester
