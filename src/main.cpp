#include <exception>
#include <iostream>

#include "brake_tester/app.hpp"

int main() {
  try {
    brake_tester::App app("brake_tester.db");
    app.run();
    return 0;
  } catch (const std::exception& startupException) {
    std::cerr << "Application startup failure: " << startupException.what() << '\n';
    return 1;
  }
}
