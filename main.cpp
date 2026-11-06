#include "RCalHeaderFramework.h"

#include <curses.h>
#include <clocale>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace {
struct Terminal {
    Terminal() {
        if (!initscr()) throw std::runtime_error("cannot initialize terminal");
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
    }
    ~Terminal() { endwin(); }
};
}

int main(int argc, char** argv) {
    try {
        std::setlocale(LC_ALL, "");
        rcal::RCalHeaderFramework framework;
        if (argc > 1) framework.loadConfigFile(argv[1]);
        Terminal terminal;
        do {
            erase();
            framework.render(stdscr);
            refresh();
        } while (framework.handleInput(getch(), stdscr) != rcal::InputResult::quit);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
