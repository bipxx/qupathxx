#pragma once

#include <optional>
#include <string>
#include <utility>

namespace qpx::ui {
// Private presentation context: the public GuiOptions contract stays frozen.
// The application entry point owns this scope on the GUI thread.
struct DesktopApplicationScope;
inline thread_local const DesktopApplicationScope* desktop_application=nullptr;
struct DesktopApplicationScope {
    const std::optional<std::string> startup_reader_options;
    const DesktopApplicationScope* const previous=desktop_application;
    explicit DesktopApplicationScope(std::optional<std::string> options)
        : startup_reader_options(std::move(options)) {desktop_application=this;}
    DesktopApplicationScope(const DesktopApplicationScope&)=delete;
    DesktopApplicationScope& operator=(const DesktopApplicationScope&)=delete;
    ~DesktopApplicationScope() {desktop_application=previous;}
};
}
