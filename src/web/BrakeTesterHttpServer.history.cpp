#include "brake_tester/web/BrakeTesterHttpServer.hpp"
#include "web/BrakeTesterHttpServerInternal.hpp"

#include <filesystem>
#include <fstream>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace brake_tester {
namespace {
std::string outcomeToText(TestOutcome outcome) {
  switch (outcome) {
    case TestOutcome::Pass: return "pass";
    case TestOutcome::Fail: return "fail";
    case TestOutcome::Unknown:
    default: return "unknown";
  }
}

std::optional<int> queryInt(const httplib::Request& request, const char* name) {
  if (!request.has_param(name)) {
    return std::nullopt;
  }

  try {
    return std::stoi(request.get_param_value(name));
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::string> queryString(const httplib::Request& request, const char* name) {
  if (!request.has_param(name)) {
    return std::nullopt;
  }

  const std::string value = request.get_param_value(name);
  if (value.empty() || value == "all") {
    return std::nullopt;
  }

  return value;
}
} // namespace

void BrakeTesterHttpServer::configureHistoryModule() {
  m_Impl->server->Get("/api/history", [this](const httplib::Request& request, httplib::Response& response) {
    try {
      HistoricalTestQuery query;
      query.year = queryInt(request, "year");
      query.month = queryInt(request, "month");
      query.vehicleReg = queryString(request, "vehicle");
      query.page = queryInt(request, "page").value_or(1);

      const auto queryPerPage = queryInt(request, "perPage");
      if (queryPerPage.has_value()) {
        m_LptRepository.setResultsPerPagePreference(*queryPerPage);
        query.perPage = *queryPerPage;
      } else {
        query.perPage = m_LptRepository.getResultsPerPagePreference();
      }

      const HistoricalPage page = m_LptRepository.getTests(query);

      nlohmann::json tests = nlohmann::json::array();
      for (const HistoricalTest& test : page.tests) {
        nlohmann::json item = {
            {"id", test.id},
            {"createdAtUtc", test.createdAtUtc},
            {"pdfFile", test.pdfFile},
            {"outcome", outcomeToText(test.outcome)},
        };

        if (test.prnFile.has_value()) {
          item["prnFile"] = *test.prnFile;
        } else {
          item["prnFile"] = nullptr;
        }
        if (test.thumbnailFile.has_value()) {
          item["thumbnailFile"] = *test.thumbnailFile;
        } else {
          item["thumbnailFile"] = nullptr;
        }

        if (test.vehicle.has_value()) {
          item["vehicle"] = {
              {"id", test.vehicle->id},
              {"reg", test.vehicle->reg},
              {"make", test.vehicle->make.value_or("")},
              {"model", test.vehicle->model.value_or("")},
              {"mileage", test.vehicle->mileage.value_or("")},
          };
        } else {
          item["vehicle"] = nullptr;
        }

        tests.push_back(std::move(item));
      }

      nlohmann::json years = nlohmann::json::array();
      for (int year : page.filterOptions.years) {
        years.push_back(year);
      }

      nlohmann::json months = nlohmann::json::array();
      for (const HistoricalMonthOption& month : page.filterOptions.months) {
        months.push_back({{"value", month.month}, {"label", month.label}});
      }

      nlohmann::json vehicles = nlohmann::json::array();
      for (const auto& reg : page.filterOptions.vehicleRegistrations) {
        vehicles.push_back(reg);
      }

      const nlohmann::json payload = {
          {"tests", tests},
          {"page", page.page},
          {"perPage", page.perPage},
          {"totalCount", page.totalCount},
          {"filters", {{"years", years}, {"months", months}, {"vehicles", vehicles}}},
      };

      response.set_content(payload.dump(), "application/json");
    } catch (const std::exception& ex) {
      response.status = 500;
      response.set_content(nlohmann::json({{"error", ex.what()}}).dump(), "application/json");
    }
  });

  m_Impl->server->Get(R"(/api/history/(\d+))", [this](const httplib::Request& request, httplib::Response& response) {
    try {
      const int testId = std::stoi(request.matches[1].str());
      HistoricalTestDetails details;
      if (!m_LptRepository.tryGetTestDetails(testId, details)) {
        response.status = 404;
        response.set_content(nlohmann::json({{"error", "Test not found"}}).dump(), "application/json");
        return;
      }

      nlohmann::json axleRows = nlohmann::json::array();
      for (const HistoricalAxleResult& axle : details.axleResults) {
        axleRows.push_back({
            {"id", axle.id},
            {"testId", axle.testId},
            {"axleIndex", axle.axleIndex},
            {"testType", axle.testType},
            {"leftBrakeForce", axle.leftBrakeForce.has_value() ? nlohmann::json(*axle.leftBrakeForce) : nlohmann::json(nullptr)},
            {"rightBrakeForce", axle.rightBrakeForce.has_value() ? nlohmann::json(*axle.rightBrakeForce) : nlohmann::json(nullptr)},
            {"efficiency", axle.efficiency.has_value() ? nlohmann::json(*axle.efficiency) : nlohmann::json(nullptr)},
            {"imbalance", axle.imbalance.has_value() ? nlohmann::json(*axle.imbalance) : nlohmann::json(nullptr)},
            {"weight", axle.weight.has_value() ? nlohmann::json(*axle.weight) : nlohmann::json(nullptr)},
        });
      }

      nlohmann::json testJson = {
          {"id", details.test.id},
          {"createdAtUtc", details.test.createdAtUtc},
          {"pdfFile", details.test.pdfFile},
          {"outcome", outcomeToText(details.test.outcome)},
      };
      if (details.test.prnFile.has_value()) {
        testJson["prnFile"] = *details.test.prnFile;
      } else {
        testJson["prnFile"] = nullptr;
      }
      if (details.test.thumbnailFile.has_value()) {
        testJson["thumbnailFile"] = *details.test.thumbnailFile;
      } else {
        testJson["thumbnailFile"] = nullptr;
      }
      if (details.test.vehicle.has_value()) {
        testJson["vehicle"] = {
            {"reg", details.test.vehicle->reg},
            {"make", details.test.vehicle->make.value_or("")},
            {"model", details.test.vehicle->model.value_or("")},
            {"mileage", details.test.vehicle->mileage.value_or("")},
        };
      } else {
        testJson["vehicle"] = nullptr;
      }

      const nlohmann::json payload = {
          {"test", testJson},
          {"axleResults", axleRows},
          {"pdfUrl", "/api/history/" + std::to_string(testId) + "/pdf"},
      };
      response.set_content(payload.dump(), "application/json");
    } catch (const std::exception& ex) {
      response.status = 500;
      response.set_content(nlohmann::json({{"error", ex.what()}}).dump(), "application/json");
    }
  });

  m_Impl->server->Delete(R"(/api/history/(\d+))", [this](const httplib::Request& request, httplib::Response& response) {
    try {
      const int testId = std::stoi(request.matches[1].str());
      const bool deleted = m_LptRepository.deleteTest(testId);
      response.set_content(nlohmann::json({{"deleted", deleted}}).dump(), "application/json");
    } catch (const std::exception& ex) {
      response.status = 500;
      response.set_content(nlohmann::json({{"error", ex.what()}}).dump(), "application/json");
    }
  });

  m_Impl->server->Get(R"(/api/history/(\d+)/pdf)", [this](const httplib::Request& request, httplib::Response& response) {
    try {
      const int testId = std::stoi(request.matches[1].str());
      HistoricalTestDetails details;
      if (!m_LptRepository.tryGetTestDetails(testId, details)) {
        response.status = 404;
        response.set_content("Test not found", "text/plain");
        return;
      }

      const std::filesystem::path pdfPath = std::filesystem::path(details.test.pdfFile);
      if (!std::filesystem::exists(pdfPath)) {
        response.status = 404;
        response.set_content("PDF not found", "text/plain");
        return;
      }

      std::ifstream stream(pdfPath, std::ios::binary);
      std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
      response.set_content(content, "application/pdf");
    } catch (const std::exception& ex) {
      response.status = 500;
      response.set_content(ex.what(), "text/plain");
    }
  });
}

} // namespace brake_tester
