#pragma once

#include <cctype>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

struct AxleResult {
  enum class ResultType {
    Service,
    Handbrake
  };

  uint8_t axleId{};
  ResultType type{};
  std::pair<uint16_t, uint16_t> brakeForce{};
  std::pair<bool, bool> stalled{};
  float imbalancePct{};
  uint16_t weightKgs{};
};

template <typename IteratorType>
class prnAnalyzer {
  IteratorType m_Begin;
  IteratorType m_End;

  static bool isValidChar(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) || c == '.' || c == '*';
  }

  static bool isNumberToken(const std::string& s) {
    if (s.empty()) {
      return false;
    }
    for (char c : s) {
      if (!std::isdigit(static_cast<unsigned char>(c))) {
        return false;
      }
    }
    return true;
  }

  static bool isDotsToken(const std::string& s) {
    return s == "...";
  }

  static uint16_t parseU16OrZero(const std::string& token) {
    if (isDotsToken(token)) {
      return 0;
    }

    if (!isNumberToken(token)) {
      throw std::runtime_error("Expected numeric token or ...");
    }

    const unsigned long value = std::stoul(token);
    if (value > 65535UL) {
      throw std::runtime_error("Numeric token out of uint16_t range");
    }

    return static_cast<uint16_t>(value);
  }

  static float parseFloatOrZero(const std::string& token) {
    if (isDotsToken(token)) {
      return 0.0f;
    }

    return std::stof(token);
  }

  std::string getNextWord() {
    while (m_Begin != m_End && !isValidChar(*m_Begin)) {
      ++m_Begin;
    }

    std::string result;
    while (m_Begin != m_End && isValidChar(*m_Begin)) {
      result += *m_Begin;
      ++m_Begin;
    }
    return result;
  }

  std::optional<std::string> nextToken() {
    const std::string tok = getNextWord();
    if (tok.empty()) {
      return std::nullopt;
    }
    return tok;
  }

  static bool isAxleStartToken(const std::string& tok) {
    return tok == "AXLE" || tok == "EAXLE";
  }

  static bool isFinalEvalToken(const std::string& tok) {
    return tok == "Final" || tok == "EFinal";
  }

  // Cheap one-token pushback.
  bool m_HasLookahead = false;
  std::string m_Lookahead;

  std::optional<std::string> nextTokenBuffered() {
    if (m_HasLookahead) {
      m_HasLookahead = false;
      return m_Lookahead;
    }
    return nextToken();
  }

  std::string requireTokenBuffered() {
    auto tok = nextTokenBuffered();
    if (!tok) {
      throw std::runtime_error("Unexpected end of PRN");
    }
    return *tok;
  }

  void pushBackToken(std::string tok) {
    if (m_HasLookahead) {
      throw std::runtime_error("Only one token of pushback supported");
    }
    m_HasLookahead = true;
    m_Lookahead = std::move(tok);
  }

  void parseMaxBrakeForce(AxleResult& result) {
    // We have already consumed "Max"
    const std::string tok2 = requireTokenBuffered(); // brake
    const std::string tok3 = requireTokenBuffered(); // force
    const std::string tok4 = requireTokenBuffered(); // Left

    if (tok2 != "brake" || tok3 != "force" || tok4 != "Left") {
      throw std::runtime_error("Malformed 'Max brake force Left' section");
    }

    const std::string leftValTok = requireTokenBuffered();
    result.brakeForce.first = parseU16OrZero(leftValTok);

    const std::string leftUnitTok = requireTokenBuffered(); // kgf
    if (leftUnitTok != "kgf") {
      throw std::runtime_error("Expected kgf after left brake force");
    }

    result.stalled.first = false;
    {
      const std::string maybeStarOrRight = requireTokenBuffered();
      if (maybeStarOrRight == "*") {
        result.stalled.first = true;
      } else {
        pushBackToken(maybeStarOrRight);
      }
    }

    const std::string rightTok = requireTokenBuffered();
    if (rightTok != "Right") {
      throw std::runtime_error("Expected Right after left brake force");
    }

    const std::string rightValTok = requireTokenBuffered();
    result.brakeForce.second = parseU16OrZero(rightValTok);

    const std::string rightUnitTok = requireTokenBuffered(); // kgf
    if (rightUnitTok != "kgf") {
      throw std::runtime_error("Expected kgf after right brake force");
    }

    result.stalled.second = false;
    {
      auto maybe = nextTokenBuffered();
      if (maybe && *maybe == "*") {
        result.stalled.second = true;
      } else if (maybe) {
        pushBackToken(*maybe);
      }
    }
  }

  void parseImbalance(AxleResult& result) {
    // "Imbalance" already consumed
    const std::string valueTok = requireTokenBuffered();
    result.imbalancePct = parseFloatOrZero(valueTok);
  }

  void parseAxleWeight(AxleResult& result) {
    // "Axle" already consumed
    const std::string weightTok = requireTokenBuffered();
    if (weightTok != "weight") {
      throw std::runtime_error("Expected 'weight' after 'Axle'");
    }

    const std::string valueTok = requireTokenBuffered();
    result.weightKgs = parseU16OrZero(valueTok);

    const std::string unitTok = requireTokenBuffered();
    if (unitTok != "kg") {
      throw std::runtime_error("Expected kg after axle weight");
    }
  }

public:
  prnAnalyzer() = delete;

  explicit prnAnalyzer(IteratorType begin, IteratorType end)
      : m_Begin(begin), m_End(end) {
    constexpr std::size_t magicNumberOffset = 0x28ba;
    for (std::size_t i = 0; i < magicNumberOffset; ++i) {
      if (m_Begin == m_End) {
        throw std::runtime_error("PRN file too small");
      }
      ++m_Begin;
    }
  }

  std::optional<AxleResult> findNextAxleResult() {
    while (true) {
      auto tokOpt = nextTokenBuffered();
      if (!tokOpt) {
        return std::nullopt;
      }

      const std::string& tok = *tokOpt;

      if (isFinalEvalToken(tok)) {
        return std::nullopt;
      }

      if (!isAxleStartToken(tok)) {
        continue;
      }

      AxleResult result{};

      // axle id
      {
        const std::string axleIdTok = requireTokenBuffered();
        const unsigned long axleId = std::stoul(axleIdTok);
        if (axleId > 255UL) {
          throw std::runtime_error("Axle ID out of range");
        }
        result.axleId = static_cast<uint8_t>(axleId);
      }

      // brake type
      {
        const std::string firstTypeTok = requireTokenBuffered();
        if (firstTypeTok == "Service") {
          const std::string secondTypeTok = requireTokenBuffered();
          if (secondTypeTok != "Brake") {
            throw std::runtime_error("Expected 'Brake' after 'Service'");
          }
          result.type = AxleResult::ResultType::Service;
        } else if (firstTypeTok == "Hand") {
          const std::string secondTypeTok = requireTokenBuffered();
          if (secondTypeTok != "Brake") {
            throw std::runtime_error("Expected 'Brake' after 'Hand'");
          }
          result.type = AxleResult::ResultType::Handbrake;
        } else if (firstTypeTok == "Handbrake") {
          result.type = AxleResult::ResultType::Handbrake;
        } else {
          throw std::runtime_error("Unknown axle brake type");
        }
      }

      bool gotBrakeForce = false;
      bool gotImbalance = false;
      bool gotWeight = false;

      while (true) {
        auto fieldTokOpt = nextTokenBuffered();
        if (!fieldTokOpt) {
          break;
        }

        const std::string& fieldTok = *fieldTokOpt;

        if (isFinalEvalToken(fieldTok)) {
          return (gotBrakeForce && gotImbalance && gotWeight) ? std::optional<AxleResult>{result} : std::nullopt;
        }

        if (isAxleStartToken(fieldTok)) {
          pushBackToken(fieldTok);
          break;
        }

        if (fieldTok == "Max") {
          parseMaxBrakeForce(result);
          gotBrakeForce = true;
        } else if (fieldTok == "Imbalance") {
          parseImbalance(result);
          gotImbalance = true;
        } else if (fieldTok == "Axle") {
          parseAxleWeight(result);
          gotWeight = true;
        } else {
          // Ignore everything else: Pedal Force, Ovality, F, etc.
        }
      }

      if (gotBrakeForce && gotImbalance && gotWeight) {
        return result;
      }
    }
  }
};
