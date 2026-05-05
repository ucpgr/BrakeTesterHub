#pragma once

#include <sqlite3.h>

#include "brake_tester/logging.hpp"

namespace brake_tester {

class DatabaseSchema {
public:
  DatabaseSchema(sqlite3* databaseHandle, SharedLogger log);
  void ensureCreated() const;

private:
  sqlite3* m_DatabaseHandle;
  SharedLogger m_Log;
};

} // namespace brake_tester
