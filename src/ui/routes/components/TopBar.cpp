#include "brake_tester/ui/routes/components/TopBar.hpp"

#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>

namespace brake_tester::ui::routes::components {
std::unique_ptr<Wt::WContainerWidget> createTopBar() {
  auto topBar = std::make_unique<Wt::WContainerWidget>();
  topBar->setStyleClass("navbar navbar-expand-lg border-bottom bg-body-tertiary px-3 py-2 gap-3");

  auto onlinePill = topBar->addWidget(std::make_unique<Wt::WText>(
      R"(<span class=\"badge rounded-pill text-bg-success\">Connected</span>)",
      Wt::TextFormat::UnsafeXHTML));
  onlinePill->setInline(false);

  auto status = topBar->addWidget(std::make_unique<Wt::WContainerWidget>());
  status->setStyleClass("flex-grow-1 text-body-secondary text-truncate");
  status->addWidget(std::make_unique<Wt::WText>("status messages"));

  auto nav = topBar->addWidget(std::make_unique<Wt::WContainerWidget>());
  nav->setStyleClass("btn-group");
  nav->setAttributeValue("role", "group");

  nav->addWidget(std::make_unique<Wt::WText>(
      R"(<button type=\"button\" class=\"btn btn-outline-secondary btn-sm\">live</button>)",
      Wt::TextFormat::UnsafeXHTML));
  nav->addWidget(std::make_unique<Wt::WText>(
      R"(<button type=\"button\" class=\"btn btn-outline-secondary btn-sm\">settings</button>)",
      Wt::TextFormat::UnsafeXHTML));
  nav->addWidget(std::make_unique<Wt::WText>(
      R"(<button type=\"button\" class=\"btn btn-outline-secondary btn-sm\">history</button>)",
      Wt::TextFormat::UnsafeXHTML));

  return topBar;
}
} // namespace brake_tester::ui::routes::components
