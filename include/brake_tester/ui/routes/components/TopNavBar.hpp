#pragma once

#include <chrono>
#include <string>

#include <Wt/WContainerWidget.h>

#include "brake_tester/ui/routes/components/StatusMessage.hpp"

namespace Wt {
class WMenu;
class WNavigationBar;
}

namespace brake_tester::ui::routes::components {

class ConnectionIndicator;
class StatusMessage;

class TopNavBar : public Wt::WContainerWidget {
public:
  explicit TopNavBar();

  void setConnected(bool connected);
  void setStatus(const std::string& message,
                 StatusPriority priority,
                 std::chrono::seconds autoResetAfter = std::chrono::seconds{60});

private:
  Wt::WNavigationBar* m_navBar{nullptr};
  ConnectionIndicator* m_connectionIndicator{nullptr};
  StatusMessage* m_statusMessage{nullptr};
  Wt::WMenu* m_menu{nullptr};
};

std::unique_ptr<TopNavBar> createTopBar();

} // namespace brake_tester::ui::routes::components
