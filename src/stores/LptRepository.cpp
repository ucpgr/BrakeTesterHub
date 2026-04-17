#include "brake_tester/stores/LptRepository.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <stdexcept>

namespace brake_tester {
namespace {

class StatementFinalizer {
public:
  explicit StatementFinalizer(sqlite3_stmt* statement) : m_Statement(statement) {}
  ~StatementFinalizer() {
    if (m_Statement != nullptr) {
      sqlite3_finalize(m_Statement);
    }
  }

private:
  sqlite3_stmt* m_Statement;
};

std::string normalizeReg(std::string reg) {
  for (char& character : reg) {
    character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
  }
  return reg;
}

std::string outcomeToText(TestOutcome outcome) {
  switch (outcome) {
    case TestOutcome::Pass: return "pass";
    case TestOutcome::Fail: return "fail";
    case TestOutcome::Unknown:
    default: return "unknown";
  }
}

TestOutcome textToOutcome(const std::string& outcome) {
  if (outcome == "pass") {
    return TestOutcome::Pass;
  }
  if (outcome == "fail") {
    return TestOutcome::Fail;
  }
  return TestOutcome::Unknown;
}

int clampResultsPerPage(int value) {
  if (value < 10) {
    return 10;
  }
  if (value > 100) {
    return 100;
  }
  return value;
}

} // namespace

LptRepository::LptRepository(sqlite3* databaseHandle, SharedLogger log)
    : m_DatabaseHandle(databaseHandle), m_Log(std::move(log)) {
  if (m_DatabaseHandle == nullptr) {
    throw std::invalid_argument("LptRepository requires a valid sqlite3 handle");
  }
  if (m_Log) {
    m_Log->information("[LptRepository Info]: Initializing schema.");
  }
  initializeSchema();
}

void LptRepository::initializeSchema() const {
  static constexpr const char* createHistoricalVehiclesSql =
      "CREATE TABLE IF NOT EXISTS historical_vehicles ("
      "id INTEGER PRIMARY KEY,"
      "reg TEXT NOT NULL UNIQUE,"
      "make TEXT,"
      "model TEXT,"
      "mileage TEXT"
      ");";

  static constexpr const char* createTestsSql =
      "CREATE TABLE IF NOT EXISTS tests ("
      "id INTEGER PRIMARY KEY,"
      "created_at_utc TEXT NOT NULL,"
      "prn_file TEXT,"
      "pdf_file TEXT NOT NULL,"
      "outcome TEXT NOT NULL DEFAULT 'unknown',"
      "historical_vehicle_id INTEGER,"
      "FOREIGN KEY (historical_vehicle_id) REFERENCES historical_vehicles(id) ON DELETE SET NULL"
      ");";

  static constexpr const char* createAxleResultsSql =
      "CREATE TABLE IF NOT EXISTS axle_results ("
      "id INTEGER PRIMARY KEY,"
      "test_id INTEGER NOT NULL,"
      "axle_index INTEGER NOT NULL,"
      "test_type TEXT NOT NULL,"
      "left_brake_force INTEGER,"
      "right_brake_force INTEGER,"
      "efficiency REAL,"
      "imbalance REAL,"
      "weight INTEGER,"
      "FOREIGN KEY (test_id) REFERENCES tests(id) ON DELETE CASCADE"
      ");";

  static constexpr const char* createPreferenceSql =
      "CREATE TABLE IF NOT EXISTS history_preferences ("
      "id INTEGER PRIMARY KEY CHECK (id = 1),"
      "results_per_page INTEGER NOT NULL"
      ");";

  std::array<const char*, 4> statements = {
      createHistoricalVehiclesSql,
      createTestsSql,
      createAxleResultsSql,
      createPreferenceSql,
  };

  for (const char* statementText : statements) {
    char* errorMessage = nullptr;
    if (sqlite3_exec(m_DatabaseHandle, statementText, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
      const std::string message = (errorMessage != nullptr) ? errorMessage : "Unknown sqlite error";
      sqlite3_free(errorMessage);
      throw std::runtime_error(message);
    }
  }

  sqlite3_exec(m_DatabaseHandle, "ALTER TABLE tests ADD COLUMN created_at_utc TEXT", nullptr, nullptr, nullptr);
  sqlite3_exec(m_DatabaseHandle, "ALTER TABLE tests ADD COLUMN prn_file TEXT", nullptr, nullptr, nullptr);
  sqlite3_exec(m_DatabaseHandle, "ALTER TABLE tests ADD COLUMN pdf_file TEXT", nullptr, nullptr, nullptr);
  sqlite3_exec(m_DatabaseHandle, "ALTER TABLE tests ADD COLUMN outcome TEXT NOT NULL DEFAULT 'unknown'", nullptr, nullptr, nullptr);
  sqlite3_exec(m_DatabaseHandle, "ALTER TABLE tests ADD COLUMN historical_vehicle_id INTEGER", nullptr, nullptr, nullptr);

  sqlite3_exec(m_DatabaseHandle,
               "UPDATE tests SET prn_file = prnFile WHERE prn_file IS NULL AND prnFile IS NOT NULL",
               nullptr,
               nullptr,
               nullptr);
  sqlite3_exec(m_DatabaseHandle,
               "UPDATE tests SET pdf_file = pdfFile WHERE pdf_file IS NULL AND pdfFile IS NOT NULL",
               nullptr,
               nullptr,
               nullptr);
  sqlite3_exec(m_DatabaseHandle,
               "UPDATE tests SET created_at_utc = datetime('now') WHERE created_at_utc IS NULL",
               nullptr,
               nullptr,
               nullptr);

  sqlite3_exec(m_DatabaseHandle,
               "INSERT INTO history_preferences (id, results_per_page) VALUES (1, 20) "
               "ON CONFLICT(id) DO NOTHING",
               nullptr,
               nullptr,
               nullptr);

  cleanupUnreferencedHistoricalVehicles();

  if (m_Log) {
    m_Log->information("[LptRepository Info]: Schema is ready (tests, axle_results, historical_vehicles, history_preferences).");
  }
}

int LptRepository::ensureHistoricalVehicle(const HistoricalVehicle& vehicle) const {
  const std::string normalizedReg = normalizeReg(vehicle.reg);
  if (normalizedReg.empty()) {
    return 0;
  }

  static constexpr const char* upsertVehicleSql =
      "INSERT INTO historical_vehicles (reg, make, model, mileage) VALUES (?, ?, ?, ?) "
      "ON CONFLICT(reg) DO UPDATE SET "
      "make = excluded.make, "
      "model = excluded.model, "
      "mileage = excluded.mileage";

  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, upsertVehicleSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare historical vehicle upsert");
  }
  StatementFinalizer statementFinalizer(statement);

  sqlite3_bind_text(statement, 1, normalizedReg.c_str(), -1, SQLITE_TRANSIENT);
  if (vehicle.make.has_value() && !vehicle.make->empty()) {
    sqlite3_bind_text(statement, 2, vehicle.make->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(statement, 2);
  }
  if (vehicle.model.has_value() && !vehicle.model->empty()) {
    sqlite3_bind_text(statement, 3, vehicle.model->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(statement, 3);
  }
  if (vehicle.mileage.has_value() && !vehicle.mileage->empty()) {
    sqlite3_bind_text(statement, 4, vehicle.mileage->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(statement, 4);
  }

  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw std::runtime_error("Failed to upsert historical vehicle");
  }

  static constexpr const char* selectVehicleSql = "SELECT id FROM historical_vehicles WHERE reg = ? LIMIT 1";
  sqlite3_stmt* selectStatement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, selectVehicleSql, -1, &selectStatement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare historical vehicle select");
  }
  StatementFinalizer selectStatementFinalizer(selectStatement);

  sqlite3_bind_text(selectStatement, 1, normalizedReg.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(selectStatement) == SQLITE_ROW) {
    return sqlite3_column_int(selectStatement, 0);
  }

  return 0;
}

int LptRepository::createTest(const HistoricalTest& test, const std::vector<HistoricalAxleResult>& axleResults) {
  sqlite3_exec(m_DatabaseHandle, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
  sqlite3_exec(m_DatabaseHandle, "BEGIN IMMEDIATE TRANSACTION", nullptr, nullptr, nullptr);

  try {
    int historicalVehicleId = 0;
    if (test.vehicle.has_value() && !test.vehicle->reg.empty()) {
      historicalVehicleId = ensureHistoricalVehicle(*test.vehicle);
    }

    static constexpr const char* insertTestSql =
        "INSERT INTO tests (created_at_utc, prn_file, pdf_file, outcome, historical_vehicle_id) VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt* insertTestStatement = nullptr;
    if (sqlite3_prepare_v2(m_DatabaseHandle, insertTestSql, -1, &insertTestStatement, nullptr) != SQLITE_OK) {
      throw std::runtime_error("Failed to prepare test insert statement");
    }
    StatementFinalizer insertTestFinalizer(insertTestStatement);

    sqlite3_bind_text(insertTestStatement, 1, test.createdAtUtc.c_str(), -1, SQLITE_TRANSIENT);
    if (test.prnFile.has_value() && !test.prnFile->empty()) {
      sqlite3_bind_text(insertTestStatement, 2, test.prnFile->c_str(), -1, SQLITE_TRANSIENT);
    } else {
      sqlite3_bind_null(insertTestStatement, 2);
    }
    sqlite3_bind_text(insertTestStatement, 3, test.pdfFile.c_str(), -1, SQLITE_TRANSIENT);
    const std::string outcome = outcomeToText(test.outcome);
    sqlite3_bind_text(insertTestStatement, 4, outcome.c_str(), -1, SQLITE_TRANSIENT);
    if (historicalVehicleId > 0) {
      sqlite3_bind_int(insertTestStatement, 5, historicalVehicleId);
    } else {
      sqlite3_bind_null(insertTestStatement, 5);
    }

    if (sqlite3_step(insertTestStatement) != SQLITE_DONE) {
      throw std::runtime_error("Failed to insert test");
    }

    const int testId = static_cast<int>(sqlite3_last_insert_rowid(m_DatabaseHandle));

    static constexpr const char* insertAxleSql =
        "INSERT INTO axle_results (test_id, axle_index, test_type, left_brake_force, right_brake_force, efficiency, imbalance, weight) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

    for (const HistoricalAxleResult& axle : axleResults) {
      sqlite3_stmt* insertAxleStatement = nullptr;
      if (sqlite3_prepare_v2(m_DatabaseHandle, insertAxleSql, -1, &insertAxleStatement, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare axle result insert statement");
      }
      StatementFinalizer insertAxleFinalizer(insertAxleStatement);

      sqlite3_bind_int(insertAxleStatement, 1, testId);
      sqlite3_bind_int(insertAxleStatement, 2, axle.axleIndex);
      sqlite3_bind_text(insertAxleStatement, 3, axle.testType.c_str(), -1, SQLITE_TRANSIENT);

      if (axle.leftBrakeForce.has_value()) {
        sqlite3_bind_int(insertAxleStatement, 4, *axle.leftBrakeForce);
      } else {
        sqlite3_bind_null(insertAxleStatement, 4);
      }

      if (axle.rightBrakeForce.has_value()) {
        sqlite3_bind_int(insertAxleStatement, 5, *axle.rightBrakeForce);
      } else {
        sqlite3_bind_null(insertAxleStatement, 5);
      }

      if (axle.efficiency.has_value()) {
        sqlite3_bind_double(insertAxleStatement, 6, *axle.efficiency);
      } else {
        sqlite3_bind_null(insertAxleStatement, 6);
      }

      if (axle.imbalance.has_value()) {
        sqlite3_bind_double(insertAxleStatement, 7, *axle.imbalance);
      } else {
        sqlite3_bind_null(insertAxleStatement, 7);
      }

      if (axle.weight.has_value()) {
        sqlite3_bind_int(insertAxleStatement, 8, *axle.weight);
      } else {
        sqlite3_bind_null(insertAxleStatement, 8);
      }

      if (sqlite3_step(insertAxleStatement) != SQLITE_DONE) {
        throw std::runtime_error("Failed to insert axle result");
      }
    }

    sqlite3_exec(m_DatabaseHandle, "COMMIT", nullptr, nullptr, nullptr);
    return testId;
  } catch (...) {
    sqlite3_exec(m_DatabaseHandle, "ROLLBACK", nullptr, nullptr, nullptr);
    throw;
  }
}

HistoricalFilterOptions LptRepository::buildFilterOptions(const HistoricalTestQuery& query) const {
  HistoricalFilterOptions options;

  static constexpr const char* yearSql =
      "SELECT DISTINCT CAST(strftime('%Y', created_at_utc) AS INTEGER) AS year_value "
      "FROM tests "
      "ORDER BY year_value DESC";
  sqlite3_stmt* yearStatement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, yearSql, -1, &yearStatement, nullptr) == SQLITE_OK) {
    StatementFinalizer yearFinalizer(yearStatement);
    while (sqlite3_step(yearStatement) == SQLITE_ROW) {
      options.years.push_back(sqlite3_column_int(yearStatement, 0));
    }
  }

  std::string monthSql =
      "SELECT DISTINCT CAST(strftime('%m', tests.created_at_utc) AS INTEGER) AS month_value "
      "FROM tests "
      "LEFT JOIN historical_vehicles hv ON hv.id = tests.historical_vehicle_id "
      "WHERE 1=1";
  if (query.year.has_value()) {
    monthSql += " AND CAST(strftime('%Y', tests.created_at_utc) AS INTEGER) = " + std::to_string(*query.year);
  }
  if (query.vehicleReg.has_value() && !query.vehicleReg->empty()) {
    monthSql += " AND hv.reg = ?";
  }
  monthSql += " ORDER BY month_value ASC";

  sqlite3_stmt* monthStatement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, monthSql.c_str(), -1, &monthStatement, nullptr) == SQLITE_OK) {
    StatementFinalizer monthFinalizer(monthStatement);
    if (query.vehicleReg.has_value() && !query.vehicleReg->empty()) {
      sqlite3_bind_text(monthStatement, 1, query.vehicleReg->c_str(), -1, SQLITE_TRANSIENT);
    }
    while (sqlite3_step(monthStatement) == SQLITE_ROW) {
      const int monthValue = sqlite3_column_int(monthStatement, 0);
      options.months.push_back(HistoricalMonthOption{monthValue, monthLabel(monthValue)});
    }
  }

  std::string vehicleSql =
      "SELECT DISTINCT hv.reg "
      "FROM tests "
      "JOIN historical_vehicles hv ON hv.id = tests.historical_vehicle_id "
      "WHERE 1=1";
  if (query.year.has_value()) {
    vehicleSql += " AND CAST(strftime('%Y', tests.created_at_utc) AS INTEGER) = " + std::to_string(*query.year);
  }
  if (query.month.has_value()) {
    vehicleSql += " AND CAST(strftime('%m', tests.created_at_utc) AS INTEGER) = " + std::to_string(*query.month);
  }
  vehicleSql += " ORDER BY hv.reg ASC";

  sqlite3_stmt* vehicleStatement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, vehicleSql.c_str(), -1, &vehicleStatement, nullptr) == SQLITE_OK) {
    StatementFinalizer vehicleFinalizer(vehicleStatement);
    while (sqlite3_step(vehicleStatement) == SQLITE_ROW) {
      options.vehicleRegistrations.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(vehicleStatement, 0)));
    }
  }

  return options;
}

HistoricalPage LptRepository::getTests(const HistoricalTestQuery& query) const {
  HistoricalPage pageResult;
  const int safePerPage = clampResultsPerPage(query.perPage);
  const int safePage = std::max(1, query.page);

  pageResult.page = safePage;
  pageResult.perPage = safePerPage;

  std::string fromClause =
      " FROM tests "
      "LEFT JOIN historical_vehicles hv ON hv.id = tests.historical_vehicle_id "
      "WHERE 1=1";

  std::vector<std::string> filters;
  if (query.year.has_value()) {
    filters.push_back(" AND CAST(strftime('%Y', tests.created_at_utc) AS INTEGER) = ?");
  }
  if (query.month.has_value()) {
    filters.push_back(" AND CAST(strftime('%m', tests.created_at_utc) AS INTEGER) = ?");
  }
  if (query.vehicleReg.has_value() && !query.vehicleReg->empty()) {
    filters.push_back(" AND hv.reg = ?");
  }

  for (const auto& filter : filters) {
    fromClause += filter;
  }

  const std::string countSql = "SELECT COUNT(*)" + fromClause;
  sqlite3_stmt* countStatement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, countSql.c_str(), -1, &countStatement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare history count statement");
  }
  StatementFinalizer countFinalizer(countStatement);

  int bindIndex = 1;
  if (query.year.has_value()) {
    sqlite3_bind_int(countStatement, bindIndex++, *query.year);
  }
  if (query.month.has_value()) {
    sqlite3_bind_int(countStatement, bindIndex++, *query.month);
  }
  if (query.vehicleReg.has_value() && !query.vehicleReg->empty()) {
    sqlite3_bind_text(countStatement, bindIndex++, query.vehicleReg->c_str(), -1, SQLITE_TRANSIENT);
  }

  if (sqlite3_step(countStatement) == SQLITE_ROW) {
    pageResult.totalCount = sqlite3_column_int(countStatement, 0);
  }

  std::string selectSql =
      "SELECT tests.id, tests.created_at_utc, tests.prn_file, tests.pdf_file, tests.outcome, "
      "hv.id, hv.reg, hv.make, hv.model, hv.mileage" +
      fromClause +
      " ORDER BY tests.created_at_utc DESC LIMIT ? OFFSET ?";

  sqlite3_stmt* selectStatement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, selectSql.c_str(), -1, &selectStatement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare history list statement");
  }
  StatementFinalizer selectFinalizer(selectStatement);

  bindIndex = 1;
  if (query.year.has_value()) {
    sqlite3_bind_int(selectStatement, bindIndex++, *query.year);
  }
  if (query.month.has_value()) {
    sqlite3_bind_int(selectStatement, bindIndex++, *query.month);
  }
  if (query.vehicleReg.has_value() && !query.vehicleReg->empty()) {
    sqlite3_bind_text(selectStatement, bindIndex++, query.vehicleReg->c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(selectStatement, bindIndex++, safePerPage);
  sqlite3_bind_int(selectStatement, bindIndex, (safePage - 1) * safePerPage);

  while (sqlite3_step(selectStatement) == SQLITE_ROW) {
    HistoricalTest test;
    test.id = sqlite3_column_int(selectStatement, 0);
    test.createdAtUtc = reinterpret_cast<const char*>(sqlite3_column_text(selectStatement, 1));
    if (sqlite3_column_type(selectStatement, 2) != SQLITE_NULL) {
      test.prnFile = std::string(reinterpret_cast<const char*>(sqlite3_column_text(selectStatement, 2)));
    }
    test.pdfFile = reinterpret_cast<const char*>(sqlite3_column_text(selectStatement, 3));
    test.outcome = textToOutcome(reinterpret_cast<const char*>(sqlite3_column_text(selectStatement, 4)));

    if (sqlite3_column_type(selectStatement, 5) != SQLITE_NULL) {
      HistoricalVehicle vehicle;
      vehicle.id = sqlite3_column_int(selectStatement, 5);
      if (sqlite3_column_type(selectStatement, 6) != SQLITE_NULL) {
        vehicle.reg = reinterpret_cast<const char*>(sqlite3_column_text(selectStatement, 6));
      }
      if (sqlite3_column_type(selectStatement, 7) != SQLITE_NULL) {
        vehicle.make = std::string(reinterpret_cast<const char*>(sqlite3_column_text(selectStatement, 7)));
      }
      if (sqlite3_column_type(selectStatement, 8) != SQLITE_NULL) {
        vehicle.model = std::string(reinterpret_cast<const char*>(sqlite3_column_text(selectStatement, 8)));
      }
      if (sqlite3_column_type(selectStatement, 9) != SQLITE_NULL) {
        vehicle.mileage = std::string(reinterpret_cast<const char*>(sqlite3_column_text(selectStatement, 9)));
      }
      test.vehicle = vehicle;
    }

    pageResult.tests.push_back(std::move(test));
  }

  pageResult.filterOptions = buildFilterOptions(query);
  return pageResult;
}

bool LptRepository::tryGetTestDetails(int testId, HistoricalTestDetails& details) const {
  static constexpr const char* selectTestSql =
      "SELECT tests.id, tests.created_at_utc, tests.prn_file, tests.pdf_file, tests.outcome, "
      "hv.id, hv.reg, hv.make, hv.model, hv.mileage "
      "FROM tests "
      "LEFT JOIN historical_vehicles hv ON hv.id = tests.historical_vehicle_id "
      "WHERE tests.id = ? LIMIT 1";

  sqlite3_stmt* selectTestStatement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, selectTestSql, -1, &selectTestStatement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare test details statement");
  }
  StatementFinalizer testFinalizer(selectTestStatement);

  sqlite3_bind_int(selectTestStatement, 1, testId);
  if (sqlite3_step(selectTestStatement) != SQLITE_ROW) {
    return false;
  }

  HistoricalTest test;
  test.id = sqlite3_column_int(selectTestStatement, 0);
  test.createdAtUtc = reinterpret_cast<const char*>(sqlite3_column_text(selectTestStatement, 1));
  if (sqlite3_column_type(selectTestStatement, 2) != SQLITE_NULL) {
    test.prnFile = std::string(reinterpret_cast<const char*>(sqlite3_column_text(selectTestStatement, 2)));
  }
  test.pdfFile = reinterpret_cast<const char*>(sqlite3_column_text(selectTestStatement, 3));
  test.outcome = textToOutcome(reinterpret_cast<const char*>(sqlite3_column_text(selectTestStatement, 4)));

  if (sqlite3_column_type(selectTestStatement, 5) != SQLITE_NULL) {
    HistoricalVehicle vehicle;
    vehicle.id = sqlite3_column_int(selectTestStatement, 5);
    if (sqlite3_column_type(selectTestStatement, 6) != SQLITE_NULL) {
      vehicle.reg = reinterpret_cast<const char*>(sqlite3_column_text(selectTestStatement, 6));
    }
    if (sqlite3_column_type(selectTestStatement, 7) != SQLITE_NULL) {
      vehicle.make = std::string(reinterpret_cast<const char*>(sqlite3_column_text(selectTestStatement, 7)));
    }
    if (sqlite3_column_type(selectTestStatement, 8) != SQLITE_NULL) {
      vehicle.model = std::string(reinterpret_cast<const char*>(sqlite3_column_text(selectTestStatement, 8)));
    }
    if (sqlite3_column_type(selectTestStatement, 9) != SQLITE_NULL) {
      vehicle.mileage = std::string(reinterpret_cast<const char*>(sqlite3_column_text(selectTestStatement, 9)));
    }
    test.vehicle = vehicle;
  }

  static constexpr const char* selectAxlesSql =
      "SELECT id, test_id, axle_index, test_type, left_brake_force, right_brake_force, efficiency, imbalance, weight "
      "FROM axle_results WHERE test_id = ? ORDER BY axle_index ASC, id ASC";

  sqlite3_stmt* selectAxleStatement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, selectAxlesSql, -1, &selectAxleStatement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare axle details statement");
  }
  StatementFinalizer axleFinalizer(selectAxleStatement);

  sqlite3_bind_int(selectAxleStatement, 1, testId);

  std::vector<HistoricalAxleResult> axles;
  while (sqlite3_step(selectAxleStatement) == SQLITE_ROW) {
    HistoricalAxleResult axle;
    axle.id = sqlite3_column_int(selectAxleStatement, 0);
    axle.testId = sqlite3_column_int(selectAxleStatement, 1);
    axle.axleIndex = sqlite3_column_int(selectAxleStatement, 2);
    axle.testType = reinterpret_cast<const char*>(sqlite3_column_text(selectAxleStatement, 3));
    if (sqlite3_column_type(selectAxleStatement, 4) != SQLITE_NULL) {
      axle.leftBrakeForce = sqlite3_column_int(selectAxleStatement, 4);
    }
    if (sqlite3_column_type(selectAxleStatement, 5) != SQLITE_NULL) {
      axle.rightBrakeForce = sqlite3_column_int(selectAxleStatement, 5);
    }
    if (sqlite3_column_type(selectAxleStatement, 6) != SQLITE_NULL) {
      axle.efficiency = sqlite3_column_double(selectAxleStatement, 6);
    }
    if (sqlite3_column_type(selectAxleStatement, 7) != SQLITE_NULL) {
      axle.imbalance = sqlite3_column_double(selectAxleStatement, 7);
    }
    if (sqlite3_column_type(selectAxleStatement, 8) != SQLITE_NULL) {
      axle.weight = sqlite3_column_int(selectAxleStatement, 8);
    }
    axles.push_back(std::move(axle));
  }

  details.test = std::move(test);
  details.axleResults = std::move(axles);
  return true;
}

bool LptRepository::deleteTest(int testId) {
  static constexpr const char* deleteSql = "DELETE FROM tests WHERE id = ?";
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, deleteSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare delete test statement");
  }
  StatementFinalizer finalizer(statement);

  sqlite3_bind_int(statement, 1, testId);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw std::runtime_error("Failed to delete test");
  }

  const bool deleted = sqlite3_changes(m_DatabaseHandle) > 0;
  if (deleted) {
    cleanupUnreferencedHistoricalVehicles();
  }
  return deleted;
}

int LptRepository::getResultsPerPagePreference() const {
  static constexpr const char* selectSql = "SELECT results_per_page FROM history_preferences WHERE id = 1";
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, selectSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare results per page select");
  }
  StatementFinalizer finalizer(statement);

  if (sqlite3_step(statement) == SQLITE_ROW) {
    return clampResultsPerPage(sqlite3_column_int(statement, 0));
  }

  return 20;
}

void LptRepository::setResultsPerPagePreference(int value) {
  const int clamped = clampResultsPerPage(value);

  static constexpr const char* upsertSql =
      "INSERT INTO history_preferences (id, results_per_page) VALUES (1, ?) "
      "ON CONFLICT(id) DO UPDATE SET results_per_page = excluded.results_per_page";
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(m_DatabaseHandle, upsertSql, -1, &statement, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare results per page upsert");
  }
  StatementFinalizer finalizer(statement);

  sqlite3_bind_int(statement, 1, clamped);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw std::runtime_error("Failed to persist results per page preference");
  }
}

void LptRepository::cleanupUnreferencedHistoricalVehicles() const {
  static constexpr const char* deleteSql =
      "DELETE FROM historical_vehicles WHERE id NOT IN ("
      "SELECT DISTINCT historical_vehicle_id FROM tests WHERE historical_vehicle_id IS NOT NULL"
      ")";
  sqlite3_exec(m_DatabaseHandle, deleteSql, nullptr, nullptr, nullptr);
}

std::string LptRepository::monthLabel(int month) {
  static const std::array<const char*, 12> monthLabels = {
      "January", "February", "March", "April", "May", "June",
      "July", "August", "September", "October", "November", "December"};

  if (month < 1 || month > 12) {
    return "Unknown";
  }

  return monthLabels[static_cast<std::size_t>(month - 1)];
}

} // namespace brake_tester
