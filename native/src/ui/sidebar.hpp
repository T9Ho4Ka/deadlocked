#pragma once

namespace dl {
struct AppState;
}

namespace dl::ui {

/// Draws the left hand navigation: title block, the tab list, and the status footer.
void draw_sidebar(AppState& state);

}  // namespace dl::ui
