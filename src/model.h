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
    explicit Model(Webview *webview, Config *config, int char_index = 0);
    ~Model();
    
    bool LoadModel(std::string model_path);
    
    gpt_params GetGPTParams(void); 
    bool SetGPTParams(gpt_params new_params, bool update_seed = false, int32_t *new_seed = 0);
    
    bool GenerateOutput(std::string prompt);
    bool RegenerateOutput(void);

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
    
    std::string old_input;
    std::vector<uint8_t> old_state;
    int n_past = 0;
    int old_n_past = 0;
    std::vector<llama_token> last_n_tokens;
    std::vector<llama_token> old_last_n_tokens;
    int n_outputs; // how many outputs we have generated
    
    inline static Webview *webview; // used for communication with the HTML UI
    inline static Config *config; // pointer to config class
    
    bool is_interacting = false;
    bool busy = false;
    std::atomic_flag stop = ATOMIC_FLAG_INIT;
    std::atomic_flag pause = ATOMIC_FLAG_INIT;
    
    std::string new_input;
    std::mutex new_input_mutex;

    int n_consumed;
    int char_index; // which character this models handles? 0 - first character
        
    inline static console_state con_st;
    std::vector<llama_token> evaluated_tokens;
};

#endif // MODEL_H
