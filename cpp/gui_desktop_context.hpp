#pragma once

namespace qpx::ui {
// Private presentation context: the public GuiOptions contract stays frozen.
// The application entry point owns this scope; embedded sessions default Close.
inline thread_local bool desktop_application=false;
struct DesktopApplicationScope {
    bool previous=desktop_application;
    DesktopApplicationScope() {desktop_application=true;}
    ~DesktopApplicationScope() {desktop_application=previous;}
};
}
