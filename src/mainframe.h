#ifndef MAINFRAME_H
#define MAINFRAME_H

#include <filesystem>
#include <set>
#include <thread>

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/cmdline.h>
#include <wx/event.h>
#include <wx/filesys.h>
#include <wx/gbsizer.h>
#include <wx/log.h>
#include <wx/display.h>

#include <stddef.h>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

#include "loguru.hpp"

#include "llama.h"

#include "config.h"
#include "model.h"
#include "webview.h"
#include "utils.h"

#define DEFAULT_CONFIG_FILE "configs/config.json"
#define ICON_PATH "resources/icon.png"


class MainFrame: public wxFrame {
public:
    MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size, int argc, wchar_t **argv);

private:
    void OnExit(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    
    void OnMouseWheel(wxMouseEvent& event);

    void OnSaveConversation(wxCommandEvent& event);
    void OnLoadConfig(wxCommandEvent& event);
    void OnSaveConfig(wxCommandEvent& event);
    void OnSaveAsConfig(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnGenerate(wxCommandEvent& event);    
    void OnPause(wxCommandEvent& event);
    void OnStop(wxCommandEvent& event);
    void OnReloadUI(wxCommandEvent& event);

    void WebviewOnLoaded(wxWebViewEvent& event);

    void WebviewCommand(wxWebViewEvent& event); 
    
    bool InitializeModels(void);
    void CreateModelList(void);
    bool SaveConfig(std::string file_path);
    bool SaveJSON(json j, std::string file_path);

    void SetUIParameters(void);

    wxDECLARE_EVENT_TABLE();
    
    Config *config;
    std::vector<Model *> models;
    Webview *webview;
    
    std::string config_file; // current configuration file
    std::set<std::string> model_files; // list of available model files
};


enum {
    ID_Hello = 1,
    BUTTON_Generate = 2,
    BUTTON_Pause = 3,
    BUTTON_Stop = 4,
    BUTTON_Reload_UI = 5,
    MENU_Generate = 6,
    MENU_Pause = 7,
    MENU_Stop = 8,
    MENU_LOAD = 9,
    MENU_SAVE = 10,
    MENU_SAVE_AS = 11,
    MENU_SAVE_CONVERSATION = 12,
    MENU_Reload_UI = wxID_HIGHEST + 1
};


static const wxCmdLineEntryDesc cmdline_desc [] = {
     {wxCMD_LINE_SWITCH, "h", "help", "displays help on the command line parameters",
        wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP},
     {wxCMD_LINE_OPTION, "m", "model", "path to language model",
        wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
     {wxCMD_LINE_OPTION, "p", "prompt", "prompt to give to model",
        wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
     {wxCMD_LINE_OPTION, "c", "config", "path to configuration file",
        wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
     {wxCMD_LINE_NONE}
};

#endif // MAINFRAME_H
