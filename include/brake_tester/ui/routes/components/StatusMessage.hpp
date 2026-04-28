#pragma once

#include <chrono>
#include <string>

#include <Wt/WContainerWidget.h>

namespace Wt {
class WPushButton;
class WTimer;
}

namespace brake_tester::ui::routes::components {

enum class StatusPriority {
  Idle,
  Low,
  Medium,
  High,
};

class StatusMessage : public Wt::WContainerWidget {
public:
  explicit StatusMessage();

  void setStatus(const std::string& message,
                 StatusPriority priority,
                 std::chrono::seconds autoResetAfter = std::chrono::seconds{60});

  [[nodiscard]] StatusPriority priority() const;
  [[nodiscard]] const std::string& message() const;

private:
  void applyVisualState();
  void openStatusDialog();
  static std::string priorityLabel(StatusPriority priority);

  std::string m_message{"Idle"};
  StatusPriority m_priority{StatusPriority::Idle};
  Wt::WPushButton* m_button{nullptr};
  Wt::WTimer* m_resetTimer{nullptr};
};

} // namespace brake_tester::ui::routes::components
