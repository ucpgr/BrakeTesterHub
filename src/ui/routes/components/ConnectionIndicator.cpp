#include "brake_tester/ui/routes/components/ConnectionIndicator.hpp"

#include <Wt/WText.h>

namespace brake_tester::ui::routes::components {

ConnectionIndicator::ConnectionIndicator() {
  setStyleClass("d-inline-flex align-items-center gap-2 text-light flex-shrink-0");

  m_dot = addWidget(std::make_unique<Wt::WText>("<span class=\"bt-connection-dot bt-connection-dot--online\"></span>",
                                                Wt::TextFormat::UnsafeXHTML));
  m_dot->setInline(false);

  m_label = addWidget(std::make_unique<Wt::WText>());
  m_label->setStyleClass("small fw-semibold d-none d-sm-inline");

  refreshView();
}

void ConnectionIndicator::setConnected(bool connected) {
  if (m_connected == connected) {
    return;
  }

  m_connected = connected;
  refreshView();
}

bool ConnectionIndicator::isConnected() const {
  return m_connected;
}

void ConnectionIndicator::refreshView() {
  if (m_dot != nullptr) {
    const auto dotClass = m_connected ? "bt-connection-dot bt-connection-dot--online"
                                      : "bt-connection-dot bt-connection-dot--offline";
    m_dot->setText("<span class=\"" + std::string(dotClass) + "\"></span>");
  }

  if (m_label != nullptr) {
    m_label->setText(m_connected ? "Connected" : "Offline");
    m_label->setStyleClass(std::string("small fw-semibold d-none d-sm-inline ") +
                           (m_connected ? "text-success" : "text-danger"));
  }
}

} // namespace brake_tester::ui::routes::components
