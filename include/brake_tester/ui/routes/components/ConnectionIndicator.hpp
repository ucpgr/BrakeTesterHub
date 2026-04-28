#pragma once

#include <string>

#include <Wt/WContainerWidget.h>

namespace Wt {
class WText;
}

namespace brake_tester::ui::routes::components {

class ConnectionIndicator : public Wt::WContainerWidget {
public:
  explicit ConnectionIndicator();

  void setConnected(bool connected);
  bool isConnected() const;

private:
  void refreshView();

  bool m_connected{true};
  Wt::WText* m_dot{nullptr};
  Wt::WText* m_label{nullptr};
};

} // namespace brake_tester::ui::routes::components
