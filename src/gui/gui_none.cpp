#include "gui.h"
bool fa_gui_available() { return false; }
void fa_gui_run(const std::string&, const std::map<std::string, std::string>&, const std::function<GuiWidget()>&,
                const std::function<void(int, int)>&) {}
void fa_gui_editor_set(const std::string&, const std::string&) {}
std::string fa_gui_editor_get(const std::string&) { return ""; }
bool fa_gui_editor_dirty(const std::string&) { return false; }
void fa_gui_editor_clean(const std::string&) {}
void fa_gui_editor_info(const std::string&, int& line, int& column, int& lines) { line = 1; column = 1; lines = 1; }
