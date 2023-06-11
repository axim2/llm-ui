#ifndef LLM_UI_H
#define LLM_UI_H

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/cmdline.h>
#include <wx/event.h>
#include <wx/filesys.h>
#include <wx/gbsizer.h>
#include <wx/log.h>

#include "loguru.hpp"

#include "mainframe.h"


class LLMUI: public wxApp {
public:
    virtual bool OnInit();
    virtual int OnExit();
    
private:
    MainFrame *frame;

};

DECLARE_APP(LLMUI)
wxIMPLEMENT_APP(LLMUI);

#endif // LLM_UI_H
