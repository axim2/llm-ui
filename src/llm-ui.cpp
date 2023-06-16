#include "llm-ui.h"


bool LLMUI::OnInit() {
    
    // turn off initial log output and init logger
    loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
    loguru::init(wxApp::argc, wxApp::argv);

    this->frame = new MainFrame("LLM-UI", wxPoint(0, 0), wxSize(0, 0), 
                              wxApp::argc, wxApp::argv);
    this->frame->Show(true);

    return true;
}


int LLMUI::OnExit() {
    return 0;
}
