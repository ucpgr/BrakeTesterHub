#pragma once

#include <sqlite3.h>

#include "brake_tester/interfaces.hpp"
#include "brake_tester/logging.hpp"

namespace brake_tester {

class LptRepository final : public ILptRepository {
public:
  LptRepository(sqlite3* databaseHandle, SharedLogger log);

private:
  void initializeSchema() const;

  sqlite3* m_DatabaseHandle;
  SharedLogger m_Log;
};

} // namespace brake_tester
