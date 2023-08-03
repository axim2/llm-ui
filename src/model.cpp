#include "llama.cpp" // this contains struct definitions for some reason??
#define LLAMA_VOCAB
#include "model.h"


Model::Model(Webview *webview, Config *config, int char_index) {
    
    this->webview = webview;
    this->config = config;
    this->char_index = char_index;
    
    // just in case
    this->stop.clear();
    this->pause.clear();
    
    llama_init_backend(false);
}


Model::~Model() {
    if (this->ctx)
        llama_free(this->ctx);
}

// loads LLM model
bool Model::LoadModel(std::string model_path) {
    
    LOG_S(INFO) << "Loading model: " << this->char_index << "\n";
    this->params.model = model_path;

    auto lparams = llama_context_default_params();

    lparams.n_ctx = this->params.n_ctx;
    lparams.seed = this->params.seed;
    lparams.f16_kv = this->params.memory_f16;
    lparams.use_mmap = this->params.use_mmap;
    lparams.use_mlock = this->params.use_mlock;

    if (this->ctx) // free old context if it exists
        llama_free(this->ctx);

    try {
        this->ctx = llama_init_from_file(this->params.model.c_str(), lparams);
    } catch (...) {
        LOG_S(ERROR) << "Error loading model: " << model_path;
        return false;
    }
    
    if (this->ctx == NULL) {
        fprintf(stderr, "%s: error: failed to load model '%s'\n", __func__, this->params.model.c_str());
        return false;
    }
    
    // check if lora is used
    if (!this->params.lora_adapter.empty()) {
    int err = llama_apply_lora_from_file(this->ctx,
                                         this->params.lora_adapter.c_str(),
                                         this->params.lora_base.empty() ? NULL : this->params.lora_base.c_str(),
                                         this->params.n_threads);
        if (err != 0) {
            fprintf(stderr, "%s: error: failed to apply lora adapter\n", __func__);
            return false;
        }
    }

    this->PrintGPTParams();

    return true;
}


// used by UI to query current params
gpt_params Model::GetGPTParams(void) {
    return this->params;
}


// used by UI to set new parameters
// if update_seed is true and random seed is generated, store it to new_seed
bool Model::SetGPTParams(gpt_params new_params, bool update_seed, uint32_t *new_seed) {
    
    // check if seed has been updated, if yes update ctx->rng
    if (update_seed) {
        int tmp_seed;
        if (new_params.seed != params.seed) {
            if (new_params.seed == LLAMA_DEFAULT_SEED) { // generate random seed and store it
                int32_t tmp_seed = std::random_device()();
                LOG_S(INFO) << "Generating random seed: " << tmp_seed;

                *new_seed = tmp_seed;
                new_params.seed = tmp_seed;
            } else {
                tmp_seed = new_params.seed;
            }
            //this->ctx->rng = std::mt19937(tmp_seed);
            this->ctx->rng.seed(tmp_seed);
        }
    }
    this->params = new_params;
    return true;
}


// generates output based on the prompt
bool Model::GenerateOutput(std::string prompt) {
    if (this->busy) {
        LOG_S(WARNING) << "LLM is already generating, returning";
        return false;
    }
    
    // just in case these were set before
    this->stop.clear();
    this->pause.clear();
        
    this->busy = true;
    
    this->n_outputs = 0;
  
    //std::string path_session = this->params.path_session;
    std::string path_session = this->params.path_prompt_cache;
    std::vector<llama_token> session_tokens;
    
    // tokenize the prompt
    this->params.prompt = prompt;
    // Add a space in front of the first character to match OG llama tokenizer behavior
    this->params.prompt.insert(0, 1, ' ');
    this->webview->GetBrowser()->RunScript("tokenizing();");
    auto embd_inp = ::llama_tokenize(ctx, params.prompt, true);
    
    const auto inp_pfx = ::llama_tokenize(ctx, "\n\n### Instruction:\n\n", true);
    const auto inp_sfx = ::llama_tokenize(ctx, "\n\n### Response:\n\n", false);
    auto llama_token_newline = ::llama_tokenize(ctx, "\n", false);
    auto user_tokens = ::llama_tokenize(ctx, this->config->user_name + ":", false);
    
    const int n_ctx = llama_n_ctx(ctx);

    if ((int) embd_inp.size() > n_ctx - 4) {
        fprintf(stderr, "%s: error: prompt is too long (%d tokens, max %d)\n", __func__, (int) embd_inp.size(), n_ctx - 4);
        return false;
    }
    
    // if n_keep == true and auto_n_keep == true, set n_keep to base prompt 
    // (before user/char lines) TODO: take into account all chars!
    if ((this->params.n_keep == 0) && (this->config->auto_n_keep)) {
        auto char_tokens = ::llama_tokenize(ctx, this->config->char_names.at(this->char_index) + ":", false);

        auto user_iter = std::search(embd_inp.begin(), embd_inp.end(), 
                                     user_tokens.begin(), user_tokens.end());
        auto user_pos = std::distance(embd_inp.begin(), user_iter);

        auto char_iter = std::search(embd_inp.begin(), embd_inp.end(), 
                                     char_tokens.begin(), char_tokens.end());
        auto char_pos = std::distance(embd_inp.begin(), char_iter);

        // set n_keep to first occurence of either char's or user's line
        this->params.n_keep = user_pos > char_pos ? char_pos : user_pos;        
        LOG_S(INFO) << "Setting n_keep based on prompt to " << this->params.n_keep;
    }
    
    // number of tokens to keep when resetting context
    if (this->params.n_keep < 0 || this->params.n_keep > (int)embd_inp.size() || this->params.instruct) {
        this->params.n_keep = (int)embd_inp.size();
    }

    // in instruct mode, we inject a prefix and a suffix to each input by the user
    /*if (this->params.instruct) {
        this->params.interactive_first = true;
        this->params.antiprompt.push_back("### Instruction:\n\n");
    }*/

    // enable interactive mode if reverse prompt or interactive start is specified
    if (this->params.antiprompt.size() != 0 || this->params.interactive_first) {
        this->params.interactive = true;
    }
    
    //printPrompt();
    fprintf(stderr, "\n");
    fprintf(stderr, "%s: char: %d, prompt: '%s'\n", __func__, this->char_index, 
            this->params.prompt.c_str());
    fprintf(stderr, "%s: number of tokens in prompt = %zu\n", __func__, embd_inp.size());
    int i;
    for (i = 0; i < (int) embd_inp.size(); i++) {
        fprintf(stderr, "%6d -> '%s'\n", embd_inp[i], llama_token_to_str(this->ctx, embd_inp[i]));
    }
    if (this->params.n_keep > 0) {
        fprintf(stderr, "%s: static prompt based on n_keep: '", __func__);
        for (int i = 0; i < this->params.n_keep; i++) {
            fprintf(stderr, "%s", llama_token_to_str(this->ctx, embd_inp[i]));
        }
        fprintf(stderr, "'\n");
    }
    fprintf(stderr, "\n");
    
    
    PrintGPTParams();
    
    // TODO: replace with ring-buffer
    this->last_n_tokens.clear();
    this->last_n_tokens.resize(n_ctx);
    std::fill(last_n_tokens.begin(), last_n_tokens.end(), 0);
    
    // store initial state
    n_past = 0;
    this->old_state.resize(llama_get_state_size(ctx));
    llama_copy_state_data(this->ctx, this->old_state.data());
    this->old_n_past = n_past;
    this->old_last_n_tokens = last_n_tokens;
    this->old_input = prompt;

    bool is_antiprompt = false;
    bool input_noecho  = false;
    
    int n_remain = params.n_predict;
    int n_consumed = 0;
    int n_session_consumed = 0;
    
    // the first thing we will do is to output the prompt, so set color accordingly
    //console_set_color(this->con_st, CONSOLE_COLOR_PROMPT);

    std::vector<llama_token> embd;

    while ((n_remain != 0 || params.interactive) && (!this->stop.test())) {
        
        while (pause.test()) // sleep for a while if paused
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // predict
        if (embd.size() > 0) {
            // infinite text generation via context swapping
            // if we run out of context:
            // - take the n_keep first tokens from the original prompt (via n_past)
            // - take half of the last (n_ctx - n_keep) tokens and recompute the logits in batches
            if (n_past + (int) embd.size() > n_ctx) {
                const int n_left = n_past - params.n_keep;
                //n_past = params.n_keep;

                // always keep the first token - BOS
                n_past = std::max(1, params.n_keep);
                
                // insert n_left/2 tokens at the start of embd from last_n_tokens
                embd.insert(embd.begin(), last_n_tokens.begin() + n_ctx - n_left/2 - embd.size(), last_n_tokens.end() - embd.size());
            }
            
            // try to reuse a matching prefix from the loaded session instead of re-eval (via n_past)
            // REVIEW
            if (n_session_consumed < (int) session_tokens.size()) {
                size_t i = 0;
                for ( ; i < embd.size(); i++) {
                    if (embd[i] != session_tokens[n_session_consumed]) {
                        session_tokens.resize(n_session_consumed);
                        break;
                    }

                    n_past++;
                    n_session_consumed++;

                    if (n_session_consumed >= (int) session_tokens.size()) {
                        break;
                    }
                }
                if (i > 0) {
                    embd.erase(embd.begin(), embd.begin() + i);
                }
            }
            
            // evaluate tokens in batches
            // embd is typically prepared beforehand to fit within a batch, but not always
            for (int i = 0; i < (int) embd.size(); i += params.n_batch) {
                int n_eval = (int) embd.size() - i;
                if (n_eval > params.n_batch) {
                    n_eval = params.n_batch;
                }
                if (llama_eval(ctx, &embd[i], n_eval, n_past, params.n_threads)) {
                    fprintf(stderr, "%s : failed to eval\n", __func__);
                    return 1;
                }
                n_past += n_eval;
            }
            
            if (embd.size() > 0 && !path_session.empty()) {
                session_tokens.insert(session_tokens.end(), embd.begin(), embd.end());
                n_session_consumed = session_tokens.size();
            }
        }

        embd.clear();
        
        if ((int)embd_inp.size() <= n_consumed && !is_interacting && !this->stop.test()) {
            //this->webview->getBrowser()->RunScript("updateStatusbar('Generating reply');");

            // out of user input, sample next token
            const float   temp            = params.temp;
            const int32_t top_k           = params.top_k <= 0 ? llama_n_vocab(ctx) : params.top_k;
            const float   top_p           = params.top_p;
            const float   tfs_z           = params.tfs_z;
            const float   typical_p       = params.typical_p;
            const int32_t repeat_last_n   = params.repeat_last_n < 0 ? n_ctx : params.repeat_last_n;
            const float   repeat_penalty  = params.repeat_penalty;
            const float   alpha_presence  = params.presence_penalty;
            const float   alpha_frequency = params.frequency_penalty;
            const int     mirostat        = params.mirostat;
            const float   mirostat_tau    = params.mirostat_tau;
            const float   mirostat_eta    = params.mirostat_eta;
            const bool    penalize_nl     = params.penalize_nl;

            llama_token id = 0;

            {
                auto logits = llama_get_logits(ctx);
                auto n_vocab = llama_n_vocab(ctx);

                // Apply params.logit_bias map
                for (auto it = params.logit_bias.begin(); it != params.logit_bias.end(); it++) {
                    logits[it->first] += it->second;
                }

                std::vector<llama_token_data> candidates;
                candidates.reserve(n_vocab);
                for (llama_token token_id = 0; token_id < n_vocab; token_id++) {
                    candidates.emplace_back(llama_token_data{token_id, logits[token_id], 0.0f});
                }

                llama_token_data_array candidates_p = { candidates.data(), candidates.size(), false };

                // Apply penalties
                float nl_logit = logits[llama_token_nl()];
                auto last_n_repeat = std::min(std::min((int)last_n_tokens.size(), repeat_last_n), n_ctx);
                llama_sample_repetition_penalty(ctx, &candidates_p,
                    last_n_tokens.data() + last_n_tokens.size() - last_n_repeat,
                    last_n_repeat, repeat_penalty);
                llama_sample_frequency_and_presence_penalties(ctx, &candidates_p,
                    last_n_tokens.data() + last_n_tokens.size() - last_n_repeat,
                    last_n_repeat, alpha_frequency, alpha_presence);
                if (!penalize_nl) {
                    logits[llama_token_nl()] = nl_logit;
                }

                if (temp <= 0) {
                    // Greedy sampling
                    id = llama_sample_token_greedy(ctx, &candidates_p);
                } else {
                    if (mirostat == 1) {
                        static float mirostat_mu = 2.0f * mirostat_tau;
                        const int mirostat_m = 100;
                        llama_sample_temperature(ctx, &candidates_p, temp);
                        id = llama_sample_token_mirostat(ctx, &candidates_p, mirostat_tau, mirostat_eta, mirostat_m, &mirostat_mu);
                    } else if (mirostat == 2) {
                        static float mirostat_mu = 2.0f * mirostat_tau;
                        llama_sample_temperature(ctx, &candidates_p, temp);
                        id = llama_sample_token_mirostat_v2(ctx, &candidates_p, mirostat_tau, mirostat_eta, &mirostat_mu);
                    } else {
                        // Temperature sampling
                        llama_sample_top_k(ctx, &candidates_p, top_k, 1);
                        llama_sample_tail_free(ctx, &candidates_p, tfs_z, 1);
                        llama_sample_typical(ctx, &candidates_p, typical_p, 1);
                        llama_sample_top_p(ctx, &candidates_p, top_p, 1);
                        llama_sample_temperature(ctx, &candidates_p, temp);
                        id = llama_sample_token(ctx, &candidates_p);
                    }
                }
                // printf("`%d`", candidates_p.size);

                last_n_tokens.erase(last_n_tokens.begin());
                last_n_tokens.push_back(id);
            }

            // replace end of text token with newline token when in interactive mode
            if (id == llama_token_eos() && params.interactive && !params.instruct) {
                id = llama_token_newline.front();
                if (params.antiprompt.size() != 0) {
                    // tokenize and inject first reverse prompt
                    const auto first_antiprompt = ::llama_tokenize(ctx, params.antiprompt.front(), false);
                    embd_inp.insert(embd_inp.end(), first_antiprompt.begin(), first_antiprompt.end());
                }
            }

            // add it to the context
            embd.push_back(id);

            // echo this to console
            input_noecho = false;

            // decrement remaining sampling budget
            --n_remain;
        } else {
            // some user input remains from prompt or interaction, forward it to processing
            while ((int)embd_inp.size() > n_consumed) {
                embd.push_back(embd_inp[n_consumed]);
                last_n_tokens.erase(last_n_tokens.begin());
                last_n_tokens.push_back(embd_inp[n_consumed]);
                ++n_consumed;
                if ((int) embd.size() >= params.n_batch) {
                    break;
                }
            }
        }

        // display text, don't display initial prompt (embd is equal to embd_inp)
        if (!input_noecho && (embd != embd_inp)) {
            for (auto id : embd) {
                printf("%s", llama_token_to_str(ctx, id));
                std::string output = std::string(llama_token_to_str(ctx, id));
                this->webview->AddTokenToUI(output);
            }
            fflush(stdout);
        }
        // in interactive mode, and not currently processing queued inputs;
        // check if we should prompt the user for more
        if (params.interactive && (int) embd_inp.size() <= n_consumed) {

            // check for reverse prompt
            if (params.antiprompt.size()) {
                std::string last_output;
                for (auto id : last_n_tokens) {
                    last_output += llama_token_to_str(ctx, id);
                }

                is_antiprompt = false;
                // Check if each of the reverse prompts appears at the end of the output.
                for (std::string & antiprompt : params.antiprompt) {
                    if (last_output.find(antiprompt.c_str(), last_output.length() - antiprompt.length(), antiprompt.length()) != std::string::npos) {

                        is_interacting = true;
                        is_antiprompt = true;
                        //set_console_color(this->con_st, CONSOLE_COLOR_USER_INPUT);
                        fflush(stdout);
                        break;
                    }
                }
            }

            if (n_past > 0 && is_interacting) {
                // potentially set color to indicate we are taking user input
                console_set_color(this->con_st, CONSOLE_COLOR_USER_INPUT);

#if defined (_WIN32)
                // Windows: must reactivate sigint handler after each signal
                signal(SIGINT, sigint_handler);
#endif

                if (params.instruct) {
                    printf("\n> ");
                }

                std::string buffer;
                if (!params.input_prefix.empty()) {
                    buffer += params.input_prefix;
                    printf("%s", buffer.c_str());
                }

                /*
                std::string line;
                bool another_line = true;
                do {
#if defined(_WIN32)
                    std::wstring wline;
                    if (!std::getline(std::wcin, wline)) {
                        // input stream is bad or EOF received
                        return 0;
                    }
                    win32_utf8_encode(wline, line);
#else
                    if (!std::getline(std::cin, line)) {
                        // input stream is bad or EOF received
                        return 0;
                    }
#endif
                    if (line.empty() || line.back() != '\\') {
                        another_line = false;
                    } else {
                        line.pop_back(); // Remove the continue character
                    }
                    buffer += line + '\n'; // Append the line to the result
                } while (another_line);

                // done taking input, reset color
                set_console_color(this->con_st, CONSOLE_COLOR_DEFAULT);*/
                // wait for additional input from the user
                
                // instead of reading from stdin we are waiting for new input from UI
                this->pause.test_and_set();
                this->n_outputs++;
                
                this->webview->GetBrowser()->RunScript("waitingForInput()");
                while (this->pause.test()) { // sleep for a while if paused
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    if (this->new_input.size() > 0)  { // new input received
                        this->new_input_mutex.lock();
                        buffer += this->new_input;
                                                
                        // store current state
                        this->old_state.resize(llama_get_state_size(ctx));
                        llama_copy_state_data(this->ctx, this->old_state.data());
                        this->old_n_past = n_past;
                        this->old_last_n_tokens = last_n_tokens;
                        this->old_input = this->new_input;
                        
                        this->new_input.clear();
                        this->new_input_mutex.unlock();
                        this->pause.clear(); // continue
                        this->webview->GetBrowser()->RunScript("generating();");
                    }
                }
    
                // Add tokens to embd only if the input buffer is non-empty
                // Entering a empty line lets the user pass control back
                if (buffer.length() > 1) {

                    // instruct mode: insert instruction prefix
                    if (params.instruct && !is_antiprompt) {
                        n_consumed = embd_inp.size();
                        embd_inp.insert(embd_inp.end(), inp_pfx.begin(), inp_pfx.end());
                    }

                    auto line_inp = ::llama_tokenize(ctx, buffer, false);
                    embd_inp.insert(embd_inp.end(), line_inp.begin(), line_inp.end());
                        
                    // instruct mode: insert response suffix
                    if (params.instruct) {
                        embd_inp.insert(embd_inp.end(), inp_sfx.begin(), inp_sfx.end());
                    }

                    n_remain -= line_inp.size();
                }

                input_noecho = true; // do not echo this again
            }

            if (n_past > 0) {
                is_interacting = false;
            }
        }

        // end of text token
        if (!embd.empty() && embd.back() == llama_token_eos()) {
            if (params.instruct) {
                is_interacting = true;
            } else { 
                fprintf(stderr, " [end of text]\n");
                break;
            }
        }

        // In interactive mode, respect the maximum number of tokens and drop back to user input when reached.
        if (params.interactive && n_remain <= 0 && params.n_predict != -1) {
            n_remain = params.n_predict;
            is_interacting = true;
        }
    }
    
    llama_print_timings(ctx);
    
    this->busy = false;
    this->webview->GetBrowser()->RunScript("generationStopped();");
    
    return true;
}


// pauses or resumes generation (toggles this->pause)
bool Model::ToggleGeneration(void) {
    if (!this->busy) { // can't pause if we aren't generating
        LOG_S(WARNING) << "Toggle generation called but we aren't generating";
        return false;
    }
    
    if (this->pause.test()) { // Resuming, clear
        this->pause.clear();
        this->webview->GetBrowser()->RunScript("generating();");
    } else { // Pausing, set pause flag
        this->pause.test_and_set();
        this->webview->GetBrowser()->RunScript("generationPaused();");
    }
    return true;
}


// stops generation (sets stop = true)
bool Model::StopGeneration(void) {
    this->pause.clear(); // we must unpause to stop generation properly
    this->stop.test_and_set();
    return true;
}


// adds user input from UI for the generation thread to use
bool Model::AddUserInput(std::string input) {
    // TODO: check for busy status
    LOG_S(INFO) << "Adding input for char " << this->char_index << " (" <<
        this->config->char_names[this->char_index] << "):\n" << input;
    this->new_input_mutex.lock();
    this->new_input = input;
    this->new_input_mutex.unlock();
    return true;
}


// regenerates reply
bool Model::RegenerateOutput() {
    if (!this->busy) {
        LOG_S(WARNING) << "Regenerate called but we aren't generating";
        return false;
    }
    
    uint32_t tmp_seed = std::random_device()();
    LOG_S(INFO) << "Using seed: " << tmp_seed << " for regen";
        
    if (this->n_outputs == 1) { // generate everything from scratch
        this->StopGeneration();
        while (this->GetBusy())
            sleep(0.1);

        this->ctx->rng = std::mt19937(tmp_seed); // create new rng
        
        std::thread thread(&Model::GenerateOutput, this, this->old_input);
        thread.detach();
        
    } else { // we already have generated second output
        // restore old state, it contains RNG state therefore it must be restored first
        llama_set_state_data(this->ctx, this->old_state.data());
        
        this->ctx->rng.seed(tmp_seed);
        this->n_past = this->old_n_past;
        this->last_n_tokens = this->old_last_n_tokens;
        this->AddUserInput(this->old_input);
    }

    return true;
}


bool Model::GetBusy(void) {
    return this->busy;
}

bool Model::GetPause(void) {
    return this->pause.test();
}


void Model::SetWebview(Webview *new_webview) {
    this->webview = new_webview;
}


// used for printing debug information
void Model::PrintGPTParams(void) {
    
    const int n_ctx = llama_n_ctx(this->ctx);

    fprintf(stderr, "sampling: repeat_last_n = %d, repeat_penalty = %f, presence_penalty = %f, frequency_penalty = %f, top_k = %d, tfs_z = %f, top_p = %f, typical_p = %f, temp = %f, mirostat = %d, mirostat_lr = %f, mirostat_ent = %f\n", this->params.repeat_last_n, this->params.repeat_penalty, this->params.presence_penalty, this->params.frequency_penalty, this->params.top_k, this->params.tfs_z, this->params.top_p, this->params.typical_p, this->params.temp, this->params.mirostat, this->params.mirostat_eta, this->params.mirostat_tau);
    fprintf(stderr, "generate: n_ctx = %d, n_batch = %d, n_predict = %d, n_keep = %d\n", n_ctx, this->params.n_batch, this->params.n_predict, this->params.n_keep);
    fprintf(stderr, "seed: %u\n", this->params.seed);
    fprintf(stderr, "\n\n");    
}

/*
void GGML::printPrompt(void) {
    fprintf(stderr, "\n");
    fprintf(stderr, "%s: prompt: '%s'\n", __func__, this->params.prompt.c_str());
    fprintf(stderr, "%s: number of tokens in prompt = %zu\n", __func__, this->prompt_embd.size());
    for (int i = 0; i < (int) this->prompt_embd.size(); i++) {
        fprintf(stderr, "%6d -> '%s'\n", this->prompt_embd[i], llama_token_to_str(this->ctx, this->prompt_embd[i]));
    }
    if (this->params.n_keep > 0) {
        fprintf(stderr, "%s: static prompt based on n_keep: '", __func__);
        for (int i = 0; i < this->params.n_keep; i++) {
            fprintf(stderr, "%s", llama_token_to_str(this->ctx, this->prompt_embd[i]));
        }
        fprintf(stderr, "'\n");
    }
    fprintf(stderr, "\n");
}*/
