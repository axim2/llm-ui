#include "config.h"
#include "utils.h"


Config::Config() {

}

Config::~Config() {

}

// parses configuration file
// return false on error, true on success
bool Config::ParseFile(std::string filename) {
    LOG_S(INFO) << "Loading configuration from: " << filename;

    std::string contents = utils::ReadTextFile(filename);
    if (contents.size() == 0) {
        return false;
    } else {
        bool ret = this->ParseJSON(contents);
        if (!ret)
            return false;
        
        for (int i = 0; i < this->n_chars; i++) {
            // set up reasonable defaults
            if (this->gpt_parameters.at(i).antiprompt.size() == 0)
                this->gpt_parameters.at(i).antiprompt.push_back(this->user_name + ":");
            
            // load prompts from files
            std::string s = utils::ReadTextFile(this->prompt_path.at(i));
            if (s.size() > 0) { // use this prompt
                this->gpt_parameters.at(i).prompt = s;
            }
        }
    }

    return true;
}


// parses configuration parameters from JSON
bool Config::ParseJSON(std::string input) {

    try {
    json j = json::parse(input);
    
    this->char_names        = j.value("char_names", std::vector<std::string>({DEFAULT_CHAR_NAME}));
    this->char_avatars      = j.value("char_avatars", std::vector<std::string>({DEFAULT_CHAR_AVATAR}));
    this->config_dir        = j.value("config_dir", DEFAULT_CONFIG_DIR);
    this->user_name         = j.value("user_name", DEFAULT_USER_NAME);
    this->user_avatar       = j.value("user_avatar", DEFAULT_USER_AVATAR);
    this->model_dir         = j.value("model_dir", DEFAULT_MODEL_DIR);
    this->model_file        = j.value("model_file", DEFAULT_MODEL_FILE);
    this->avatar_dir        = j.value("avatar_dir", DEFAULT_AVATAR_DIR);
        
    this->prompt_path       = j.value("prompt_path", std::vector<std::string>({DEFAULT_PROMPT_PATH}));
    this->ui_dir            = j.value("ui_dir", DEFAULT_UI_DIR);
    this->ui_style          = j.value("ui_style", DEFAULT_UI_STYLE);
    this->userscripts_dir   = j.value("userscripts_dir", DEFAULT_USERSCRIPTS_DIR);
    
    if (j.contains("n_chars"))
        this->n_chars = j["n_chars"].get<int>();
    
    this->gpt_json = j.value("gpt_params", json::object());
    this->gpt_parameters.clear(); // clear existing parameters
    uint32_t i;
    for (i = 0; i < this->gpt_json.size(); i++)
        this->gpt_parameters.push_back(this->gpt_json[i]);

    while ((int) this->gpt_parameters.size() < this->n_chars) {
        // fill the rest gpt_parameters if they aren't given in the config file
        this->gpt_parameters.push_back(this->gpt_json[i-1]); // i increased at the end of the loop
    }
    
    if (j.contains("auto_n_keep"))
        this->auto_n_keep = j["auto_n_keep"].get<bool>();


    } catch (...) {
        LOG_S(ERROR) << "Exception when parsing config JSON";
        return false;
    }
    return true;
}


// Converts all configuration data to JSON
void to_json(json& j, const Config& cfg) {
    j = json{
        {"auto_n_keep",     cfg.auto_n_keep},
        {"char_names",      cfg.char_names},
        {"char_avatars",    cfg.char_avatars},
        {"config_dir",      cfg.config_dir},
        {"user_name",       cfg.user_name},
        {"user_avatar",     cfg.user_avatar},
        {"model_dir",       cfg.model_dir},
        {"model_file",      cfg.model_file},
        {"avatar_dir",      cfg.avatar_dir},
        {"prompt_path",     cfg.prompt_path},
        {"ui_dir",          cfg.ui_dir},
        {"ui_style",        cfg.ui_style},
        {"userscripts_dir", cfg.userscripts_dir},
        {"n_chars",         cfg.n_chars},
        {"gpt_params",      cfg.gpt_parameters}
    };
}


// converts JSON to gpt_params, check wherever fields exist before attempting conversion
void from_json(const json& j, gpt_params& params) {
    try {
    // automatic conversion of types don't work, most of numbers are strings
    if (j.contains("seed"))
        params.seed             = atoi(j["seed"].get<std::string>().c_str());
    if (j.contains("n_threads"))
        params.n_threads        = atoi(j["n_threads"].get<std::string>().c_str());
    if (j.contains("n_predict"))
        params.n_predict        = atoi(j["n_predict"].get<std::string>().c_str());
    if (j.contains("n_ctx"))
        params.n_ctx            = atoi(j["n_ctx"].get<std::string>().c_str());
    if (j.contains("n_batch"))
        params.n_batch          = atoi(j["n_batch"].get<std::string>().c_str());
    if (j.contains("n_keep"))
        params.n_keep           = atoi(j["n_keep"].get<std::string>().c_str());
       
    if (j.contains("logit_bias")) {
        // logit_bias is an array
        params.logit_bias       = std::unordered_map<llama_token, float>();
        for (auto &tmp : j["logit_bias"])
            // first parameter can be either int or string
            if (tmp[0].is_string()) {
                params.logit_bias.insert(std::make_pair(
                    atoi(tmp[0].get<std::string>().c_str()),
                    atof(tmp[1].get<std::string>().c_str())
                ));
            } else { // it's an int
                params.logit_bias.insert(std::make_pair(
                    tmp[0].get<int>(),
                    atof(tmp[1].get<std::string>().c_str())
                ));                
            }
    }

    if (j.contains("top_k"))
        params.top_k            = atoi(j["top_k"].get<std::string>().c_str());
    if (j.contains("top_p"))
        params.top_p            = atof(j["top_p"].get<std::string>().c_str());
    if (j.contains("tfs_z"))
        params.tfs_z            = atof(j["tfs_z"].get<std::string>().c_str());
    if (j.contains("typical_p"))
        params.typical_p        = atof(j["typical_p"].get<std::string>().c_str());
    if (j.contains("temp"))
        params.temp             = atof(j["temp"].get<std::string>().c_str());

    if (j.contains("repeat_penalty"))
        params.repeat_penalty   = atof(j["repeat_penalty"].get<std::string>().c_str());
    if (j.contains("repeat_last_n"))
        params.repeat_last_n    = atoi(j["repeat_last_n"].get<std::string>().c_str());
    if (j.contains("frequency_penalty"))
        params.frequency_penalty = atof(j["frequency_penalty"].get<std::string>().c_str());
    if (j.contains("presence_penalty"))
        params.presence_penalty = atof(j["presence_penalty"].get<std::string>().c_str());
    if (j.contains("mirostat"))
        params.mirostat         = atoi(j["mirostat"].get<std::string>().c_str());
    if (j.contains("mirostat_tau"))
        params.mirostat_tau     = atof(j["mirostat_tau"].get<std::string>().c_str());
    if (j.contains("mirostat_eta"))
        params.mirostat_eta     = atof(j["mirostat_eta"].get<std::string>().c_str());
    if (j.contains("model"))
        params.model            = j["model"].get<std::string>();
    if (j.contains("prompt"))
        params.prompt           = j["prompt"].get<std::string>();
    if (j.contains("input_prefix"))
        params.input_prefix     = j["input_prefix"].get<std::string>();
    if (j.contains("input_suffix"))
        params.input_suffix     = j["input_suffix"].get<std::string>();

        
    if (j.contains("antiprompt")) {
        // antiprompt is an array
        params.antiprompt       = std::vector<std::string>();
        for (auto &tmp : j["antiprompt"])
            params.antiprompt.push_back(tmp);
    }

    if (j.contains("lora_adapter"))
        params.lora_adapter     = j["lora_adapter"].get<std::string>();
    if (j.contains("lora_base"))
        params.lora_base        = j["lora_base"].get<std::string>();
    if (j.contains("penalize_nl"))
        params.penalize_nl      = atoi(j["penalize_nl"].get<std::string>().c_str());

    } catch (...) {
        LOG_S(ERROR) << "Exception when converting JSON to gpt_params";
    }
}


// Converts gpt_params to JSON
void to_json(json& j, const gpt_params& params) {
    // logit_bias has floats as members, create new map with std::string instead
    std::unordered_map<llama_token, std::string> logit_tmp;
    for (auto& [key, value] : params.logit_bias) {
        logit_tmp.insert(std::make_pair(key, std::to_string(value)));
    }
    
    // float values must be converted to strings, otherwise there will be rounding errors
    // better to convert everything to strings since otherwise there might be issues later
    j = json{
        {"seed", std::to_string(params.seed)}, 
        {"n_threads", std::to_string(params.n_threads)}, 
        {"n_predict", std::to_string(params.n_predict)},
        {"n_ctx", std::to_string(params.n_ctx)},
        {"n_batch", std::to_string(params.n_batch)},
        {"n_keep", std::to_string(params.n_keep)},
        {"logit_bias", logit_tmp},
        {"top_k", std::to_string(params.top_k)},
        {"top_p", std::to_string(params.top_p)},
        {"tfs_z", std::to_string(params.tfs_z)},
        {"typical_p", std::to_string(params.typical_p)},
        {"temp", std::to_string(params.temp)},
        {"repeat_penalty", std::to_string(params.repeat_penalty)},
        {"repeat_last_n", std::to_string(params.repeat_last_n)},
        {"frequency_penalty", std::to_string(params.frequency_penalty)},
        {"presence_penalty", std::to_string(params.presence_penalty)},
        {"mirostat", std::to_string(params.mirostat)},
        {"mirostat_tau", std::to_string(params.mirostat_tau)},
        {"mirostat_eta", std::to_string(params.mirostat_eta)},
        {"model", params.model},
        {"prompt", params.prompt},
        {"input_prefix", params.input_prefix},
        {"input_suffix", params.input_suffix},
        {"antiprompt", params.antiprompt},
        {"lora_adapter", params.lora_adapter},
        {"lora_base", params.lora_base},
        {"penalize_nl", std::to_string(params.penalize_nl)}
        // other fields not used for now
    };
}
