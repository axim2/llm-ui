#include "llm-ui.h"


bool LLMUI::OnInit() {
    
    loguru::init(wxApp::argc, wxApp::argv); // init logger

    // Query available screen size and calculate reasonable dimension for the window
    int height = wxSystemSettings::GetMetric(wxSYS_SCREEN_Y);
    int width = wxSystemSettings::GetMetric(wxSYS_SCREEN_X);
    if (width > 1280) {
        width = width > 1600 ? 0.75*width : 0.9*width;
    } 
    if (height > 768) {
        height = height > 1080 ? 0.75*height : 0.9*height;
    }
    
    this->frame = new MainFrame("LLM-UI", wxPoint(0, 0), wxSize(width, height), 
                              wxApp::argc, wxApp::argv);
    this->frame->Show(true);

    return true;
}


int LLMUI::OnExit() {
    return 0;
}
