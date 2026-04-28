#include "brake_tester/ui/routes/components/StatusMessage.hpp"

#include <Wt/WBreak.h>
#include <Wt/WDialog.h>
#include <Wt/WLength.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <Wt/WTimer.h>

namespace brake_tester::ui::routes::components {

StatusMessage::StatusMessage() {
  setStyleClass("d-flex align-items-center flex-grow-1 min-w-0");

  m_button = addWidget(std::make_unique<Wt::WPushButton>());
  m_button->setStyleClass("btn btn-sm bt-status-message text-truncate w-100 text-start");
  m_button->setToolTip("Click to view full status");
  m_button->clicked().connect(this, &StatusMessage::openStatusDialog);

  m_resetTimer = addChild(std::make_unique<Wt::WTimer>());
  m_resetTimer->setSingleShot(true);
  m_resetTimer->timeout().connect([this] {
    setStatus("Idle", StatusPriority::Idle, std::chrono::seconds{0});
  });

  applyVisualState();
}

void StatusMessage::setStatus(const std::string& message,
                              StatusPriority priority,
                              std::chrono::seconds autoResetAfter) {
  m_message = message.empty() ? "Idle" : message;
  m_priority = priority;
  applyVisualState();

  m_resetTimer->stop();
  if (m_priority != StatusPriority::Idle && autoResetAfter.count() > 0) {
    m_resetTimer->setInterval(std::chrono::duration_cast<std::chrono::milliseconds>(autoResetAfter));
    m_resetTimer->start();
  }
}

StatusPriority StatusMessage::priority() const {
  return m_priority;
}

const std::string& StatusMessage::message() const {
  return m_message;
}

void StatusMessage::applyVisualState() {
  std::string priorityClass;
  switch (m_priority) {
  case StatusPriority::Idle:
    priorityClass = "btn-outline-secondary text-light-emphasis";
    break;
  case StatusPriority::Low:
    priorityClass = "btn-outline-info text-info";
    break;
  case StatusPriority::Medium:
    priorityClass = "btn-outline-warning text-warning";
    break;
  case StatusPriority::High:
    priorityClass = "btn-outline-danger text-danger";
    break;
  }

  m_button->setText(m_message);
  m_button->setAttributeValue("aria-label", "Status: " + priorityLabel(m_priority) + ". " + m_message);
  m_button->setStyleClass("btn btn-sm bt-status-message text-truncate w-100 text-start " + priorityClass);
}

void StatusMessage::openStatusDialog() {
  auto dialog = std::make_unique<Wt::WDialog>("Current status");
  dialog->setModal(true);

  auto* contents = dialog->contents();
  contents->setStyleClass("d-flex flex-column gap-2");

  contents->addWidget(std::make_unique<Wt::WText>("Priority: <strong>" + priorityLabel(m_priority) + "</strong>",
                                                   Wt::TextFormat::UnsafeXHTML));
  contents->addWidget(std::make_unique<Wt::WBreak>());
  auto details = contents->addWidget(std::make_unique<Wt::WText>(m_message));
  details->setStyleClass("text-wrap");

  auto closeButton = dialog->footer()->addWidget(std::make_unique<Wt::WPushButton>("Close"));
  closeButton->setStyleClass("btn btn-secondary");

  auto* dialogPtr = dialog.get();
  closeButton->clicked().connect([dialogPtr] {
    dialogPtr->accept();
  });

  dialogPtr->finished().connect([dialogPtr] {
    delete dialogPtr;
  });

  dialog.release()->show();
}

std::string StatusMessage::priorityLabel(StatusPriority priority) {
  switch (priority) {
  case StatusPriority::Idle:
    return "Idle";
  case StatusPriority::Low:
    return "Low";
  case StatusPriority::Medium:
    return "Medium";
  case StatusPriority::High:
    return "High";
  }

  return "Idle";
}

} // namespace brake_tester::ui::routes::components
