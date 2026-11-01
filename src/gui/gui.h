#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

// One thing in a window.
// kinds: label  button  row  column  editor  textview  listbox  spacer  shortcut
struct GuiWidget {
    std::string kind;
    std::string text;                          // label / button text, editor name, textview text, shortcut keys
    std::vector<std::string> items;            // listbox entries
    std::map<std::string, std::string> opts;   // width height grow gap padding align background color accent selected language placeholder single
    std::vector<GuiWidget> children;
    int id = -1;                               // widgets that react (button, listbox, textfield, shortcut): the number given back to onEvent
    // filled in by the layout:
    float x = 0, y = 0, w = 0, h = 0;
    std::string key;
};

// Implemented by gui_raylib.cpp (windows on screen) or gui_none.cpp (default build).
bool fa_gui_available();

// Opens a window and runs until it is closed. build() returns what the window shows right now (a column);
// it is called again after every click, key shortcut or change in a text editor.
// onEvent(id, item) is called when something reacts: item is the clicked entry of a listbox, otherwise -1.
void fa_gui_run(const std::string& title, const std::map<std::string, std::string>& windowOpts,
                const std::function<GuiWidget()>& build,
                const std::function<void(int id, int item)>& onEvent);

// Text editors keep their own text. These read and change it (they also work before the window is open).
void fa_gui_editor_set(const std::string& name, const std::string& text);
std::string fa_gui_editor_get(const std::string& name);
bool fa_gui_editor_dirty(const std::string& name);
void fa_gui_editor_clean(const std::string& name);
void fa_gui_editor_info(const std::string& name, int& line, int& column, int& lines);
