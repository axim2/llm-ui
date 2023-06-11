#ifndef WEBVIEW_H
#define WEBVIEW_H

#include <filesystem>
#include <fstream> 
#include <iostream>
#include <regex>
#include <vector>

#include <wx/log.h>
#include "wx/webview.h"
#include "wx/webviewarchivehandler.h"
#include "wx/webviewfshandler.h"

#if !wxUSE_WEBVIEW_WEBKIT && !wxUSE_WEBVIEW_WEBKIT2 && !wxUSE_WEBVIEW_IE && !wxUSE_WEBVIEW_EDGE
#error "A wxWebView backend is required"
#endif

#include "wx/fs_mem.h"

#include "loguru.hpp"

#include "utils.h"

class Webview {
public:
    explicit Webview(wxWindow *parent, const std::string ui_dir, const std::string ui_style,
                     const std::string userscript_path, const std::string avatar_path);
    ~Webview();
    
    wxWebView *GetBrowser(void);
    bool AddTokenToUI(std::string token);
    bool LoadUIFiles(void);
    bool DeleteMemoryFiles(void);

private:
    bool CheckUIStyles(std::string path);
    bool LoadFiles(std::string path, bool root=false);
    wxWebView *browser;
    
    std::string ui_dir;
    std::string ui_style;
    std::string userscript_path;
    std::string avatar_path;
    inline static std::vector<std::string> ui_styles; // list of available UI styles
    inline static std::vector<std::string> ui_style_files; // list of files related to currently active style
    inline static std::vector<std::string> memory_files; // list of all files added to memory FS
};

#endif // WEBVIEW_H
