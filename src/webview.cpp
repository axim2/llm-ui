#include "webview.h"

namespace fs = std::filesystem;

/**
 * Creates wxWebView to render the UI
 * @param ui_dir path to UI files (html, css, etc.)
 * @param ui_style selected UI style (e.g. default)
 * @param userscript_path path to scripts
 * @param avatar_path path to avatars
 */ 
Webview::Webview(wxWindow *parent, const std::string ui_dir, std::string ui_style,
                 const std::string userscript_path, const std::string avatar_path) {
    
    wxFileSystem::AddHandler(new wxMemoryFSHandler);
    
    this->ui_dir = ui_dir;
    this->ui_style = ui_style;
    this->userscript_path = userscript_path;
    this->avatar_path = avatar_path;

    this->LoadUIFiles();
    
    std::string url = "memory:index.html";
    this->browser = wxWebView::New(parent, wxID_ANY, url);
    browser->RegisterHandler(wxSharedPtr<wxWebViewHandler>(new wxWebViewFSHandler("memory")));
}


Webview::~Webview() {
    
}


wxWebView *Webview::GetBrowser() {
    return this->browser;
}


// called by LLM code
bool Webview::AddTokenToUI(std::string token) {
    token = utils::CleanStringForJS(token);
    bool result = this->browser->RunScript("LLMOutput(\"" + token + "\");");
    return result;
}


// loads UI files including userscripts and avatars
bool Webview::LoadUIFiles(void) {
    // 1. check which UI styles are available
    this->CheckUIStyles(ui_dir);
    if (this->ui_styles.size() == 0) { // no UI styles found
        wxLogError("No UI styles found, exiting...\n");
        exit(1);
    }
    
    // 2. load selected/default UI style
    if(std::find(this->ui_styles.begin(), this->ui_styles.end(), 
        this->ui_dir + this->ui_style) != this->ui_styles.end()) {
        
        this->LoadFiles(this->ui_dir + this->ui_style, true);
    } else { // specified style not found, select first available style
        
        LOG_S(WARNING) << "UI style " << this->ui_style << " not found, loading " 
            << this->ui_styles.at(0);
        this->LoadFiles(this->ui_styles.at(0), true);
    }
    
    // 3. load userscripts
    this->LoadFiles(this->userscript_path);
    
    // 4. load avatars
    this->LoadFiles(this->avatar_path);
    return true;
}


// deletes all files from the memory FS
bool Webview::DeleteMemoryFiles(void) {
    for (auto file : this->memory_files) {
        wxMemoryFSHandler::RemoveFile(file);
    }
    memory_files.clear(); // all files removed
    return true;
}


// checks which UI styles are available
bool Webview::CheckUIStyles(std::string path) {
     try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                // check that index.html exists under directory
                if (fs::exists(entry.path().string() + "/index.html")) {
                    this->ui_styles.push_back(entry.path().string());
                }
            }
        }
     } catch (...) {
        LOG_S(ERROR) << "ui_dir not found: " << path;
        return false;         
     }
     return true;
 }


/**
 * Loads all files under a given path to the memory FS
 * @param path that contains files to load
 * @param root wherever files will be stored to the root of memory FS
 */
bool Webview::LoadFiles(std::string path, bool root) {
    std::ifstream file;

     try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                
                // read it to the buffer
                file.open(entry.path().string(), std::fstream::binary);
                std::vector<char> buffer(entry.file_size());
                file.read(&buffer[0], entry.file_size());
                file.close();
                
                // if root=true, strip initial path from file path for memory FS
                std::string memory_path;
                if (root) {
                    memory_path = std::regex_replace(entry.path().string(),
                                                     std::regex(path+"/"), "");
                } else {
                    memory_path = entry.path().string();
                }
                
                // add it to the memory FS and to our records
                wxMemoryFSHandler::AddFile(memory_path, &buffer[0], entry.file_size()); 
                this->ui_style_files.push_back(memory_path);
                this->memory_files.push_back(memory_path);
            }
        }
     } catch (...) {
        LOG_S(ERROR) << "Path not found: " << path << " when loading files";
        return false;         
     }
     return true;    
}
