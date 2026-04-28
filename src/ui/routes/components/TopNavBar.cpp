#include "brake_tester/ui/routes/components/TopNavBar.hpp"

#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WMenu.h>
#include <Wt/WNavigationBar.h>
#include <Wt/WPopupMenu.h>
#include <Wt/WPushButton.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WText.h>

#include "brake_tester/ui/routes/components/ConnectionIndicator.hpp"
#include "brake_tester/ui/routes/components/StatusMessage.hpp"

namespace brake_tester::ui::routes::components {
namespace {
void navigateToInternalPath(const std::string& path) {
  if (auto* app = Wt::WApplication::instance(); app != nullptr) {
    app->setInternalPath(path, true);
  }
}
} // namespace

TopNavBar::TopNavBar() {
  setStyleClass("px-3 pt-3");

  m_navBar = addWidget(std::make_unique<Wt::WNavigationBar>());
  m_navBar->setResponsive(false);
  m_navBar->setStyleClass("navbar navbar-expand-lg navbar-dark bg-primary border border-secondary-subtle rounded-3 px-3 py-2");

  auto layoutRow = std::make_unique<Wt::WContainerWidget>();
  layoutRow->setStyleClass("d-flex align-items-center w-100 gap-3 flex-nowrap");

  auto leftSide = std::make_unique<Wt::WContainerWidget>();
  leftSide->setStyleClass("d-flex align-items-center flex-grow-1 min-w-0 gap-3");

  m_connectionIndicator = leftSide->addWidget(std::make_unique<ConnectionIndicator>());
  m_statusMessage = leftSide->addWidget(std::make_unique<StatusMessage>());

  layoutRow->addWidget(std::move(leftSide));

  auto rightSide = std::make_unique<Wt::WContainerWidget>();
  rightSide->setStyleClass("d-flex align-items-center ms-auto flex-shrink-0");

  auto* contentStack = addWidget(std::make_unique<Wt::WStackedWidget>());
  contentStack->hide();

  auto menu = std::make_unique<Wt::WMenu>(contentStack);
  m_menu = menu.get();
  m_menu->setInternalPathEnabled("/");
  m_menu->setStyleClass("navbar-nav d-none d-lg-flex flex-row gap-2");

  auto homeItem = m_menu->addItem("Home", std::make_unique<Wt::WText>("Home"));
  homeItem->setPathComponent("home");

  auto settingsItem = m_menu->addItem("Settings", std::make_unique<Wt::WText>("Settings"));
  settingsItem->setPathComponent("settings");

  auto historyItem = m_menu->addItem("History", std::make_unique<Wt::WText>("History"));
  historyItem->setPathComponent("history");

  rightSide->addWidget(std::move(menu));

  auto mobileNavButton = std::make_unique<Wt::WPushButton>("…");
  mobileNavButton->setStyleClass("btn btn-outline-light btn-sm d-inline-flex d-lg-none");
  mobileNavButton->setToolTip("Navigation");

  auto popupMenu = std::make_unique<Wt::WPopupMenu>();
  popupMenu->addItem("Home")->triggered().connect([] {
    navigateToInternalPath("/home");
  });
  popupMenu->addItem("Settings")->triggered().connect([] {
    navigateToInternalPath("/settings");
  });
  popupMenu->addItem("History")->triggered().connect([] {
    navigateToInternalPath("/history");
  });

  mobileNavButton->setMenu(std::move(popupMenu));
  rightSide->addWidget(std::move(mobileNavButton));

  layoutRow->addWidget(std::move(rightSide));

  m_navBar->addWidget(std::move(layoutRow), Wt::AlignmentFlag::Left);

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
