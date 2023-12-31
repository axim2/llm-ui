#include "mainframe.h"

namespace fs = std::filesystem;

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(MENU_SAVE_CONVERSATION,    MainFrame::OnSaveConversation)
    EVT_MENU(MENU_LOAD,                 MainFrame::OnLoadConfig)
    EVT_MENU(MENU_SAVE,                 MainFrame::OnSaveConfig)
    EVT_MENU(MENU_SAVE_AS,              MainFrame::OnSaveAsConfig)
    EVT_MENU(wxID_EXIT,                 MainFrame::OnExit)
    EVT_MENU(wxID_ABOUT,                MainFrame::OnAbout)

    EVT_MENU(MENU_Generate,             MainFrame::OnGenerate)
    EVT_MENU(MENU_Pause,                MainFrame::OnPause)
    EVT_MENU(MENU_Stop,                 MainFrame::OnStop)
    EVT_MENU(MENU_Reload_UI,            MainFrame::OnReloadUI)
    EVT_MENU(MENU_DEBUG,                MainFrame::OnDebug)

    EVT_BUTTON(BUTTON_Generate, MainFrame::OnGenerate)
    EVT_BUTTON(BUTTON_Pause, MainFrame::OnPause)
    EVT_BUTTON(BUTTON_Stop, MainFrame::OnStop)
    EVT_BUTTON(BUTTON_Reload_UI, MainFrame::OnReloadUI)
    
    EVT_CLOSE(MainFrame::OnClose)
wxEND_EVENT_TABLE()


MainFrame::MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size, 
                 int argc, wchar_t **argv)
        : wxFrame(NULL, wxID_ANY, title, pos, size) {

    // parse command line arguments:
    wxCmdLineParser parser(cmdline_desc, argc, argv);
    switch (parser.Parse()) {
        case -1: // help requested
            exit(1);
            break;
            
        case 0: // everything is ok; proceed 
            break; 
        
        default: // invalid command line parameter
            exit(1);
            break; 
    }
    
    loguru::g_stderr_verbosity = 9; // restore logging output

    // Query available screen size and calculate reasonable dimension for the window
    wxDisplay display = wxDisplay(this); // get current screen
    wxRect rect = display.GetClientArea();
    int height = rect.GetHeight();
    int width = rect.GetWidth();
    if (width > 1280) {
        width = width > 1600 ? 0.75*width : 0.9*width;
    }
    if (height > 768) {
        height = height > 1080 ? 0.9*height : 0.95*height; // height is more important for us
    }
    this->SetSize(wxDefaultCoord, wxDefaultCoord, width, height);
    LOG_S(INFO) << "Setting window size to: " << width << "x" << height;

    // handle command line parameters
    wxString tmp; // std::string doesn't work here
    if (parser.Found("c", &tmp)) { // config file
        this->config_file = tmp;
    } else { // use default value   
        this->config_file = DEFAULT_CONFIG_FILE;
    }
    this->config = new Config();
    if (!this->config->ParseFile(this->config_file)) {
        LOG_S(ERROR) << "Error parsing configuration file: " << this->config_file << ". Exiting...";
        exit(1);
    }
    
    if (parser.Found("m", &tmp)) { // model, split model path to dir and file
        std::size_t found = tmp.find_last_of("/\\");
        if (found == std::string::npos) { // not found, so we don't have dir
            this->config->model_dir = ".";
            this->config->model_file = tmp;
        } else {
            this->config->model_dir = tmp.substr(0, found);
            this->config->model_file = tmp.substr(found + 1);
        }
    }

    if (parser.Found("p", &tmp)) { // prompt
        this->config->gpt_parameters.at(0).prompt = tmp; // FIXME: handle multiple chars here
    }
    
    while (!this->InitializeModels()) { // show dialog to load model
        wxFileDialog openFileDialog(this, _("Select model file"), this->config->model_dir, "",
                       "*", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
        
        while (openFileDialog.ShowModal() == wxID_CANCEL); // show dialog until user makes a selection
        
        std::string model_path = openFileDialog.GetPath().utf8_string();
        LOG_S(INFO) << "Selected model: " << model_path;
        std::size_t found = model_path.find_last_of("/\\");
        this->config->model_dir = model_path.substr(0, found);
        this->config->model_file = model_path.substr(found + 1);
    }

    CreateModelList();

    this->webview = new Webview(this, this->config->ui_dir, this->config->ui_style,
                                this->config->userscripts_dir, this->config->avatar_dir);

    for (uint32_t i = 0; i < this->models.size(); i++) {
        this->models.at(i)->SetWebview(this->webview);
    }
    
    // build GUI
    wxInitAllImageHandlers();
    SetIcon(wxIcon(ICON_PATH, wxBITMAP_TYPE_ANY));
    
    // create menus
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(MENU_SAVE_CONVERSATION, "&Save conversation",
                     "Saves conversation as JSON to the file");
    menuFile->AppendSeparator();
    menuFile->Append(MENU_LOAD, "&Load configuration\tCtrl-O",
                     "Loads configuration from the file");
    menuFile->Append(MENU_SAVE, "&Save configuration\tCtrl-S",
                     "Saves configuration to the current configuration file");
    menuFile->Append(MENU_SAVE_AS, "&Save configuration As...\tCtrl-Shift-S",
                     "Saves configuration to the file");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);

    wxMenu *menuDebug = new wxMenu;
    menuDebug->Append(MENU_Generate, "&Generate reply\tCtrl-G");
    menuDebug->Append(MENU_Pause, "&Pause Generation\tCtrl-P");
    menuDebug->Append(MENU_Stop, "&Stop Generation");
    menuDebug->Append(MENU_Reload_UI, "&Reload UI\tCtrl-R");
    menuDebug->Append(MENU_DEBUG, "&Debug");

    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);

    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuDebug, "&Debug");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);

    CreateStatusBar();
    SetStatusText("Welcome to LLM-UI");
    
    
    // create sizer and add Webview browser to it
    wxBoxSizer *vSizer = new wxBoxSizer(wxVERTICAL);    
    vSizer->Add(this->webview->GetBrowser(), wxSizerFlags().Expand().Proportion(1));
    SetSizer(vSizer);    
    Centre();
    
    // Handle mousewheel
    this->webview->GetBrowser()->Bind(wxEVT_MOUSEWHEEL, &MainFrame::OnMouseWheel, this);
    
    // Add script message handler
    // Note: On Windows only the Edge backend supports AddScriptMessageHandler
    this->webview->GetBrowser()->AddScriptMessageHandler("command");
    this->webview->GetBrowser()->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
                                      &MainFrame::WebviewCommand, this);
    this->webview->GetBrowser()->Bind(wxEVT_WEBVIEW_LOADED, 
                                      &MainFrame::WebviewOnLoaded, this);
}


void MainFrame::OnExit(wxCommandEvent& event) {
    Close(true); // this will trigger OnClose handler below
}


void MainFrame::OnClose(wxCloseEvent& event) {
    
    for (uint32_t i = 0; i < this->models.size(); i++) {
        if (this->models.at(i)) {
            this->models.at(i)->StopGeneration();
            while (this->models.at(i)->GetBusy()) // we need to wait if model is processing prompt/input
                sleep(0.1);
            delete this->models.at(i);
        }
    }
    delete this->webview;
    
    // save current configuration
    this->SaveConfig(this->config_file);  

    Destroy();
}


// handles zooming with Ctrl + Wheel
void MainFrame::OnMouseWheel(wxMouseEvent& event) {
    
    if (!wxGetKeyState(WXK_CONTROL)) // Control isn't pressed
        return;
    
    float current_zoom = this->webview->GetBrowser()->GetZoomFactor();
    float delta = 0.1;
    
    if (event.GetWheelRotation() > 0) { // zoom in
        this->webview->GetBrowser()->SetZoomFactor(current_zoom + delta);
    } else { // zoom out
        this->webview->GetBrowser()->SetZoomFactor(current_zoom - delta);
    }
}



bool MainFrame::InitializeModels(void) {
    
    uint32_t i;
    
    // delete old models if they exist
    for (i = 0; i < this->models.size(); i++) {
        if (this->models.at(i)) {
            // if generation is running, we must stop it before deleting the model
            this->models.at(i)->StopGeneration();
            while (this->models.at(i)->GetBusy()) // we need to wait if model is processing prompt/input
                sleep(0.1);
            delete this->models.at(i);
        }        
    }
    
    // create new models
    for (i = 0; i < this->config->n_chars; i++) {
        if (this->models.size() <= i) { // create new vector element
            this->models.push_back(new Model(this->webview, this->config, i));
        } else { // use existing element
            this->models.at(i) = new Model(this->webview, this->config, i);
        }
        this->models.at(i)->SetGPTParams(this->config->gpt_parameters.at(i));
        
        // check that the model file exists
        std::ifstream file;
        std::string model_path = this->config->model_dir + "/" + this->config->model_file;
        file.open(model_path);
        if (file.good()) {
            if (this->models.at(i)->LoadModel(model_path)) {
                this->config->gpt_parameters.at(i).model = model_path;
            } else { // invalid model file
                return false;
            }
        } else { // model file not found or not accessible
            return false;
        }
    }
    
    return true;
}


// Creates a list of available models and stores it to the this->model_files
void MainFrame::CreateModelList(void) {
    try {
        for (const auto& entry : fs::directory_iterator(this->config->model_dir)) {
            if (!entry.is_directory()) {
                // do sanity check on file size
                if (entry.file_size() > 100000000) // 100MB
                    this->model_files.insert(entry.path().filename().string());
            }
        }
     } catch (...) {
        LOG_S(ERROR) << "model_dir not found: " << this->config->model_dir;
     }
}



void MainFrame::OnSaveConversation(wxCommandEvent& event) {

    for (uint32_t i = 0; i < this->models.size(); i++) {
        if (this->models.at(i)->GetBusy()) { 
            // can't save if we are busy generating and we aren't paused
            if (!this->models.at(i)->GetPause()) {
                LOG_S(WARNING) << "Can't save conversation while generating output";
                return;
            }
        }
    }

    wxString output;
    if (this->webview->GetBrowser()->RunScript("retrieveLog();", &output)) {
        
        // show save dialog
        wxFileDialog saveFileDialog(this, _("Save conversation"), "", "", 
                                    "JSON files (*.json)|*.json", wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
        
        if (saveFileDialog.ShowModal() == wxID_CANCEL)
            return;
        
        output = utils::CleanJSString(output.ToStdString());
        json j = json::parse(output);
        
        bool ret = this->SaveJSON(j, saveFileDialog.GetPath().utf8_string());
        if (!ret)
            wxLogError("Error saving conversation to: %s", saveFileDialog.GetPath());       
        
    } else { // some error happened
        LOG_S(ERROR) << "Error retrieving conversation log";
    }
}


void MainFrame::OnAbout(wxCommandEvent& event) {
    wxMessageBox("LLM-UI Version 0.1",
                "About LLM-UI", wxOK | wxICON_INFORMATION);
}


void MainFrame::OnLoadConfig(wxCommandEvent& event) {

    wxFileDialog openFileDialog(this, _("Open configuration file"), this->config->config_dir, "",
                       "*", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
 
    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return;

    this->config_file = openFileDialog.GetPath().utf8_string();
    this->config->ParseFile(this->config_file);
    InitializeModels(); // reloads the model
    SetUIParameters(); // send new config to UI
}


void MainFrame::OnSaveConfig(wxCommandEvent& event) {
    bool ret = this->SaveConfig(this->config_file);
    if (!ret)
        wxLogError("Error saving configuration: %s", this->config_file);
}


void MainFrame::OnSaveAsConfig(wxCommandEvent& event) {
    // show file selection dialog
    std::size_t found = this->config_file.find_last_of("/\\");
    std::string config_filename = this->config_file.substr(found + 1);
    wxFileDialog saveFileDialog(this, _("Save configuration file"), 
                                this->config->config_dir, config_filename,
                                "*", wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
    
    if (saveFileDialog.ShowModal() == wxID_CANCEL)
        return;
 
    bool ret = this->SaveConfig(saveFileDialog.GetPath().utf8_string());
    if (!ret)
        wxLogError("Error saving configuration: %s", saveFileDialog.GetPath());
    else
        this->config_file = saveFileDialog.GetPath().utf8_string(); // store current configuration file
}


// saves configuration to the file, return success status (true/false)
bool MainFrame::SaveConfig(std::string file_path) {
    // convert configuration to JSON format
    json params_j = *this->config;
    std::string params_s = params_j.dump(2);
    bool ret = utils::WriteTextFile(params_s, file_path);
    if (ret)
        SetStatusText("Saved configuration to " + file_path);

    return ret;
}


bool MainFrame::SaveJSON(json j, std::string file_path) {
    std::string str = j.dump(2);

    bool ret = utils::WriteTextFile(str, file_path);
    if (ret)
        SetStatusText("Saved to " + file_path);

    return ret;    
}


// for testing
void MainFrame::OnGenerate(wxCommandEvent& event) {
    std::thread thread(&Model::GenerateOutput, this->models.at(0), "Hi there");
    thread.detach();
}

void MainFrame::OnPause(wxCommandEvent& event) {
    for (uint32_t i = 0; i < this->models.size(); i++) {
        if (this->models.at(i)->GetBusy())
            this->models.at(i)->ToggleGeneration();
    }
}

void MainFrame::OnStop(wxCommandEvent& event) {
    for (uint32_t i = 0; i < this->models.size(); i++) {
        this->models.at(i)->StopGeneration();
    }
}

void MainFrame::OnReloadUI(wxCommandEvent& event) {
    // delete existing files, read all UI files again to memory and reload the browser...
    this->webview->DeleteMemoryFiles();
    this->webview->LoadUIFiles();
    this->webview->GetBrowser()->Reload();
}

// used for debugging
void MainFrame::OnDebug(wxCommandEvent& event) {
    this->models.at(0)->RegenerateOutput();

}


// process command from UI
void MainFrame::WebviewCommand(wxWebViewEvent& event) {

    uint32_t i;
    json j = json::parse(event.GetString()); // parse incoming message as JSON

    // check for commands, unfortunately C++ doesn't support switch statement on strings
    if (j["cmd"] == "start generation") {
        std::string prompt = j["params"]["prompt"];
        int n = j["params"]["char_index"].get<int>();
        prompt = utils::CleanJSString(prompt);
        std::thread thread(&Model::GenerateOutput, this->models.at(n), prompt);
        thread.detach();
        
    } else if (j["cmd"] == "continue generation") {
        int n = j["params"]["char_index"].get<int>();
        std::string input = j["params"]["input"];
        input = utils::CleanJSString(input);
        this->models.at(n)->AddUserInput(input);
        
    } else if (j["cmd"] == "toggle generation") {
        for (i = 0; i < this->models.size(); i++) {
            if (this->models.at(i)->GetBusy())
                this->models.at(i)->ToggleGeneration();
        }
        
    } else if (j["cmd"] == "stop generation") {
        for (i = 0; i < this->models.size(); i++) {
            this->models.at(i)->StopGeneration();
        }
        
    } else if (j["cmd"] == "get params") {
        SetUIParameters();
        
    } else if (j["cmd"] == "set params") {
        if (j.contains("params")) { // convert params from JSON and save them
            // JSON library doesn't support getting child objects, they must be parsed as strings
            std::string params_s = j["params"]["params"];
            params_s = utils::CleanJSString(params_s);

            //LOG_S(INFO) << "Received new params from UI: " << params_s;
            uint32_t new_seed;
            this->config->ParseJSON(params_s);
            for (i = 0; i < this->models.size(); i++) {
                new_seed = this->config->gpt_parameters.at(i).seed;
                this->models.at(i)->SetGPTParams(this->config->gpt_parameters.at(i), true, &new_seed);
                if (new_seed != this->config->gpt_parameters.at(i).seed) // seed has changed, update it
                    this->config->gpt_parameters.at(i).seed = new_seed;
            }
        }
        
    } else if (j["cmd"] == "load model") {
        if (j.contains("model")) {
            std::string old_model = this->config->model_file; // save just in case
            this->config->model_file = j["model"].get<std::string>();
            if (InitializeModels()) { // model loaded successfully
                SetUIParameters(); // send new parameters to UI
            } else { // some error
                this->webview->GetBrowser()->RunScript("updateStatusbar('Error loading model: " + this->config->model_file + "');");    
                this->config->model_file = old_model; // revert
            }
        }
        
    } else if (j["cmd"] =="regenerate") {
        int n = j["params"]["char_index"].get<int>();
        LOG_S(INFO) << "Calling renegerate on character: " << n;
        this->models.at(n)->RegenerateOutput();
        
    } else {
        LOG_S(WARNING) << "Unknown command received from UI: " << j["cmd"];
    }
        
    return;
}


void MainFrame::WebviewOnLoaded(wxWebViewEvent& event) {
    // send list of models and current settings to the Webview
    json j = this->model_files;
    this->webview->GetBrowser()->RunScript("parseModels('" + j.dump() + "');");    
    SetUIParameters();
}


// sends current parameters to the UI
void MainFrame::SetUIParameters(void) {
    json params_j = *this->config;
    
    for (uint32_t i = 0; i < this->config->n_chars; i++)
        params_j["gpt_params"][i]["prompt"] = utils::CleanStringForJS(params_j["gpt_params"][i]["prompt"]);
    std::string params_s = params_j.dump();
    this->webview->GetBrowser()->RunScript("parseParams('" + params_s + "');");
}
