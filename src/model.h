#ifndef MODEL_H
#define MODEL_H

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <vector>

#include <unistd.h>

#include <wx/log.h> 

#include "loguru.hpp"

#include "examples/common.h"
#include "ggml.h"
#include "llama.h"
#include "llama-util.h"

#include "webview.h"
#include "config.h"

/*
#ifndef LLAMA_VOCAB
#define LLAMA_VOCAB
struct llama_vocab {
    using id    = int32_t;
    using token = std::string;

    struct token_score {
        token tok;
        float score;
    }; 

    std::unordered_map<token, id> token_to_id;
    std::vector<token_score> id_to_token;
};
#endif*/


class Model {
public:
    explicit Model(Webview *webview, Config *config);
    ~Model();
    
    bool LoadModel(std::string model_path);
    
    gpt_params GetGPTParams(void); 
    bool SetGPTParams(gpt_params new_params, bool update_seed = false);
    
    bool GenerateOutput(std::string prompt);

    bool ToggleGeneration(void); 
    bool StopGeneration(void);
    
    bool AddUserInput(std::string input);
    
    bool GetBusy(void);
    
    void SetWebview(Webview *new_webview);
    
private:
    void PrintGPTParams(); // used for printing debug information
    //void PrintPrompt(); // prints prompt and associated token ids
    
    gpt_params params;
    llama_context *ctx = nullptr;
    Webview *webview; // used for communication with the HTML UI
    Config *config; // pointer to config class
    
    inline static bool is_interacting = false;
    inline static bool busy = false;
    inline static std::atomic_flag stop = ATOMIC_FLAG_INIT;
    inline static std::atomic_flag pause = ATOMIC_FLAG_INIT;
    
    inline static std::string new_input;
    std::mutex new_input_mutex;

    int n_consumed;
    std::vector<llama_token> last_n_tokens;
        
    inline static console_state con_st;
    inline static std::vector<llama_token> evaluated_tokens;
};

#endif // MODEL_H
