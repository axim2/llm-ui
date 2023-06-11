#ifndef CONFIG_H
#define CONFIG_H

#include <iostream>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

#include "loguru.hpp"

#include "examples/common.h"

#define DEFAULT_PROMPT          "Conversation between Bob the assistant and User\nUser:"
#define DEFAULT_PROMPT_DIR      "llama.cpp/prompts/"
#define DEFAULT_PROMPT_FILE     "chat-with-bob.txt"
#define DEFAULT_USER_NAME       "User"
#define DEFAULT_USER_AVATAR     "user.jpg"
#define DEFAULT_CHAR_NAME       "Bob"
#define DEFAULT_CHAR_AVATAR     "llama.jpg"
#define DEFAULT_MODEL_DIR       "models/"
#define DEFAULT_MODEL_FILE      "ggml-model.bin"
#define DEFAULT_AVATAR_DIR      "avatars/"
#define DEFAULT_CONFIG_DIR      "configs/"
#define DEFAULT_UI_DIR          "ui/"
#define DEFAULT_UI_STYLE        "default"
#define DEFAULT_USERSCRIPTS_DIR "userscripts/"

class Config {
public:
    Config();
    ~Config();
    
    bool ParseFile(std::string filename);
    bool ParseJSON(std::string json);
    
    
    std::string char_name   = DEFAULT_CHAR_NAME;
    std::string char_avatar = DEFAULT_CHAR_AVATAR;
    std::string config_dir  = DEFAULT_CONFIG_DIR;
    std::string user_name   = DEFAULT_USER_NAME;
    std::string user_avatar = DEFAULT_USER_AVATAR;
    std::string model_dir;
    std::string model_file;
    std::string avatar_dir;
    std::string prompt      = DEFAULT_PROMPT;
    std::string prompt_dir;
    std::string prompt_file = DEFAULT_PROMPT_FILE;
    std::string ui_dir      = DEFAULT_UI_DIR;
    std::string ui_style    = DEFAULT_UI_STYLE;
    std::string userscripts_dir;
    json gpt_json; // GPT params as JSON object before parsing
    gpt_params gpt_parameters; 

};

void to_json(json& j, const Config& cfg);

void from_json(const json& j, gpt_params& params);
void to_json(json& j, const gpt_params& params);

#endif // CONFIG_H