#include "app.hpp"

int main() {
    dl::App app;
    if (!app.init()) {
        return 1;
    }
    app.run();
    return 0;
}
