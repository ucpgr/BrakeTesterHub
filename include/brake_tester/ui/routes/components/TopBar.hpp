#pragma once

#include <memory>

namespace Wt {
class WContainerWidget;
}

namespace brake_tester::ui::routes::components {
std::unique_ptr<Wt::WContainerWidget> createTopBar();
}
