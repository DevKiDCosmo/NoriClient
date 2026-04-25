#pragma once

#include <functional>
#include <string>

namespace ui {

using IncomingUrlHandler = std::function<void(const std::string &url)>;

void installAppController(const std::string &version,
                          const std::string &portsSummary,
                          const std::string &appIconPath,
                          IncomingUrlHandler handler);

void runApplication();
void showInfoWindow();

} // namespace ui

