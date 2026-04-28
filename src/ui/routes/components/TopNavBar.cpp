#include "brake_tester/ui/routes/components/TopNavBar.hpp"

#include <Wt/WContainerWidget.h>
#include <Wt/WMenu.h>
#include <Wt/WNavigationBar.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WText.h>

#include "brake_tester/ui/routes/components/ConnectionIndicator.hpp"
#include "brake_tester/ui/routes/components/StatusMessage.hpp"

namespace brake_tester::ui::routes::components {

TopNavBar::TopNavBar() {
  setStyleClass("px-3 pt-3");

  m_navBar = addWidget(std::make_unique<Wt::WNavigationBar>());
  m_navBar->setResponsive(true);
  m_navBar->setStyleClass("navbar navbar-expand-lg navbar-dark bg-primary border border-secondary-subtle rounded-3");

  auto leftSide = std::make_unique<Wt::WContainerWidget>();
  leftSide->setStyleClass("d-flex align-items-center flex-grow-1 min-w-0 gap-3");

  m_connectionIndicator = leftSide->addWidget(std::make_unique<ConnectionIndicator>());
  m_statusMessage = leftSide->addWidget(std::make_unique<StatusMessage>());

  m_navBar->addWidget(std::move(leftSide), Wt::AlignmentFlag::Left);

  auto* contentStack = addWidget(std::make_unique<Wt::WStackedWidget>());
  contentStack->hide();

  auto menu = std::make_unique<Wt::WMenu>(contentStack);
  m_menu = menu.get();
  m_menu->setInternalPathEnabled("/");
  m_menu->setStyleClass("navbar-nav ms-lg-3");

  auto homeItem = m_menu->addItem("Home", std::make_unique<Wt::WText>("Home"));
  homeItem->setPathComponent("home");

  auto settingsItem = m_menu->addItem("Settings", std::make_unique<Wt::WText>("Settings"));
  settingsItem->setPathComponent("settings");

  auto historyItem = m_menu->addItem("History", std::make_unique<Wt::WText>("History"));
  historyItem->setPathComponent("history");

  m_navBar->addMenu(std::move(menu), Wt::AlignmentFlag::Right);

  setConnected(true);
  setStatus("Idle", StatusPriority::Idle, std::chrono::seconds{0});
}

void TopNavBar::setConnected(bool connected) {
  m_connectionIndicator->setConnected(connected);
}

void TopNavBar::setStatus(const std::string& message,
                          StatusPriority priority,
                          std::chrono::seconds autoResetAfter) {
  m_statusMessage->setStatus(message, priority, autoResetAfter);
}

std::unique_ptr<TopNavBar> createTopBar() {
  return std::make_unique<TopNavBar>();
}

} // namespace brake_tester::ui::routes::components
