// Base userscript
/* jshint esversion: 6 */
const input_elem = document.querySelector(".inputarea");
const user_input_elem = document.querySelector(".user-input");
const chat_elem = document.querySelector(".ui-chat");

const statusbar = document.querySelector('.statusarea');
const base_prompt_elem = document.querySelector('.base-prompt');

var settingsPopup = document.getElementById('settings-popup');
var settings_body = document.getElementById('settings-body');

// <span> element that closes the modal
const span = document.getElementsByClassName("close")[0];

const model_list = document.getElementById('model');


var params = {};
var prompt_parsed = false; // has initial prompt been parsed?
var models = {}; // list of available models
var old_model;

var base_prompt = []; // base prompt
var base_log = []; // initial messages from the log are stored here, separately for each char
var last_log_index = []; // array of ints per char denoting when the char last outputted response
var log = []; // all interactive conversations are logged here
var tmplog; // incoming tokens go here

var first_run = []; // wherever we are sending the first message to LLM
var is_generating = false; // wherever we are currently generating
var is_paused = false; // do we need this?

var current_char = 0; // which char should reply next?

// common descriptions, not used yet
var char_description; // description of AI character
var scenario; // starting scenario (same as world info?)
var char_greeting; // first message of the character


// handle user input
input_elem.addEventListener("submit", event => {
  event.preventDefault();

  if (is_generating) // model is still generating output
    return;

  const input_text = user_input_elem.value;
  if (!input_text) 
    return;
  
  user_input_elem.value = "";
  appendUserMessage(input_text);
  
  // create new texbox for output
  appendCharMessage("", current_char);
  
  // set status as generating
  is_generating = true;
  generating();

  // disable model selection, since changing model while generating may cause problems
  model_list.disabled = true; 

  // used for prompt generation
  processUserInput(input_text);
});


function tokenizing() {
  statusbar.textContent = params.char_names[current_char] + " is tokenizing and processing prompt...";    
}


function generating() {
  statusbar.textContent = params.char_names[current_char] + " is typing...";  
}


function generationPaused() {
    statusbar.textContent = params.char_names[current_char] + ": Generation paused...";
}


// used by LLM to indicate that it's waiting for input in interactive mode
function waitingForInput() {
  is_generating = false;
  model_list.disabled = false;
  if (tmplog.length > 0) {
    // check that tmplog correctly starts with the current char name
    if (!tmplog.startsWith(params.char_names[current_char] + ":"))
      tmplog = params.char_names[current_char] + ":" + tmplog;
      
    log.push(tmplog);
    tmplog="";
  }
  
  // store index of the last reply of this char
  last_log_index[current_char] = log.length;
  
  // update current_char
  current_char++;
  if (current_char == params.n_chars)
    current_char = 0;
  
  if ((current_char != 0) && (!first_run[current_char])) {
    // call next char directly without waiting for user input
    model_list.disabled = true;
    appendCharMessage("", current_char);
    processUserInput(null);
    return;
  }
  
  updateStatusbar('Reverse prompt found, waiting for input, next char: ' +
    params.char_names[current_char]);
}


// used by LLM to indicate that generation has been stopped, updates the statusbar
function generationStopped(time) {
  is_generating = false;
  model_list.disabled = false;
  // add last message to the log
  if (tmplog.length > 0) {
    log.push(tmplog);
    tmplog="";
  }

  statusbar.textContent = "Finished generating...";
  // TODO: maybe add timings here
}

function updateStatusbar(text) {
    statusbar.textContent = text;
}

// creates a message bubble for the Character, note avatar_dir must contain trailing slash
function appendCharMessage(text, char_index) {
  appendMessage(params.char_names[char_index], 
                "memory:" + params.avatar_dir + params.char_avatars[char_index], "left", text);
}

// creates a message bubble for the User
function appendUserMessage(text) {
  appendMessage(params.user_name, 
                "memory:" + params.avatar_dir + params.user_avatar, "right", text);
}

// creates a message bubble
function appendMessage(name, img, side, text) {
  const msgHTML = `<div class="msg ${side}-msg">
      <div class="msg-avatar" style="background-image: url(${img})"></div>
      <div class="msg-bubble">
        <div class="msg-info">
          <div class="msg-info-time">${formatDate(new Date())}</div>
          <div class="msg-info-name">${name}</div>
        </div>
        <div class="msg-text">${text}</div>
      </div>
  </div>`;

  chat_elem.insertAdjacentHTML("beforeend", msgHTML);
  chat_elem.scrollTop += 400;
}

function toggleGeneration() {
    let command = {};
    command.cmd = "toggle generation";
    window.command.postMessage(command);
}

function stopGeneration() {
    let command = {};
    command.cmd = "stop generation";
    window.command.postMessage(command);
    
    for (var i = 0; i < params.n_chars; i++)
      first_run[i] = true;
    prompt_parsed = false;
}

function reloadParams() {
    let command = {};
    command.cmd = "get params";
    command.params = {};
    // FIXME: remove callback?
    command.params.callback = "parseParams";
    window.command.postMessage(command);
}


function parseParams(new_params, show_ui = false) {
  if (new_params != null) {
    params = JSON.parse(new_params);
  }
    
  //statusbar.innerHTML = "Received gpt parameters, seed is: " + params.gpt_params.seed + " " + params.gpt_params.model + " antiprompt: " + params.gpt_params.antiprompt;
    
  // initialize arrays
  var i;
  for (i = 0; i < params.n_chars; i++) {
    if (first_run.length <= i)
      first_run.push(true);
    if (base_log.length <= i)
      base_log[i] = [];
    if (last_log_index.length <= i)
      last_log_index.push(0);
  }
    
  if (!prompt_parsed) {
    for (i = 0; i < params.n_chars; i++) {
      if (params.gpt_params[i].prompt.length > 0) {
        // parse initial prompt when we receive params for a first time
        prompt_parsed = true;
        parsePrompt(params.gpt_params[i].prompt, i);
      }
    }
  }
    
  // select current model from the model list
  model_list.value = params.model_file;
    
  // if model has changed, we need to provide full prompt while generating
  if (params.model_file != old_model) {
    for (i = 0; i < params.n_chars; i++)
      first_run[i] = true;
  }
  
  old_model = params.model_file;
    
  if (show_ui)
    showSettingsUI();
  
  if (params.n_chars > 1)
    updateStatusbar("Next char: " + params.char_names[current_char]);
}


// TODO: should we always reload params when showing settings? at least when changing random seed > -1!
function showSettingsUI() {

  populateSettingsWindow();
  settingsPopup.style.display = "flex";

  // simulate click to the General tab of settings popup
  var settings_element = document.getElementById("settings-button-general");
  if (settings_element)
    settings_element.click();
}

// When the user clicks on <span> (x), close the modal
span.onclick = function() {
  settingsPopup.style.display = "none";
};

// close popups
window.onclick = function(event) {
  if (event.target == settingsPopup) {
    settingsPopup.style.display = "none";
  }
};


// populate settings window
function populateSettingsWindow() {
  
  let elem, entry, entries, i, j;

  if (settings_body == null)
    window.alert("settingsBody is null");
  
  // set a name for settings button of the first character
  elem = document.getElementById("settings-button-0");
  elem.textContent = params.char_names[0];
  
  // generate tabs for other characters if necessary
  for (i = 1; i < params.n_chars; i++) {
    if (!document.getElementById("settings-" + i)) {
      addSettingsTab(i);
    }
  }

  
  // handle general settings
  elem = document.getElementById("settings-general");

  for (const [key, value] of Object.entries(params)) {

    entry = elem.getElementsByClassName(key + "-value")[0];

    if (entry != null) {
      if (key == "auto_n_keep") { // checkbox
        if (value)
          entry.checked = true;
        else
          entry.checked = false;
        
      } else {
          entry.value = value;
      }
    }
  } /* for (const... */


  // populate settings for each character
  for (i = 0; i < params.n_chars; i++) {
    elem = document.getElementById("settings-" + i);
    
    for (const [key, value] of Object.entries(params.gpt_params[i])) {
    
      entry = elem.getElementsByClassName(key + "-value")[0];

      if (entry != null) {
        
        if (key == "antiprompt") { // antiprompt is vector of strings
          entry.value = "";
          for (j = 0; j < value.length; j++) {
            entry.value += "[" + value.at(j) + "]";
          }
          
        } else if (key == "logit_bias") { // logit_bias is int => float(string) map
          entry.value = "";
              
          for (j = 0; j < value.length; j++) {
            // each item is 2-dimensional array
            entry.value += "[" + value.at(j).at(0) + "," + value.at(j).at(1) + "]";
          }
          
        } else if (entry.tagName == "TEXTAREA") { // textareas need to be handled differently
          entry.innerHTML = value;
          
        } else {
          entry.value = value;
        }
      }
    } /* for (const... */    
  } /* for (i = 0... */
}


// adds a tab containing character's settings
function addSettingsTab(char_index) {
  let parent, elem, new_elem;
  
  // create new button for selecting the tab
  let html = '<button class="settings-link" id="settings-button-' + char_index + 
    '" onclick="openSettingsTab(event, \'settings-' + char_index + '\')">' + 
    params.char_names[char_index] + '</button>';
  elem = document.getElementById("settings-header");
  elem.insertAdjacentHTML("beforeend", html);

  // get first character settings tab, clone, change id, and add it to the end
  elem = document.getElementById("settings-0");
  new_elem = elem.cloneNode(true);
  new_elem.id = "settings-" + char_index;
  settings_body.appendChild(new_elem);
}


// Parses list of models from the backend
function parseModels(models_json) {
  models = JSON.parse(models_json);

  var html;
  for (let i=0; i<models.length; i++) {
    html = "<option value='" + models.at(i) + "'>" + models.at(i) + "</option>";
    model_list.insertAdjacentHTML("beforeend", html);
  }
}


// Send command to the backed to load a selected model
function loadModel(new_model) {
  if (new_model != params.model_file) { // model has changed
    let command = {};
    command.cmd = "load model";
    command.model = new_model;
    updateStatusbar("Loading model: " + new_model + " ...");
    window.command.postMessage(command);
  }
}


// called by UI
// if input_text == null, don't include "user:" + input to the LLM parameters
function processUserInput(input_text) {
  let command = {};
    
  if (first_run[current_char]) {
    command.cmd = "start generation";
    command.params = {};
    command.params.char_index = current_char; // which character is generating the reply
        //command.params.prompt = base_prompt + "\n" + log.join("\n") + "\n" +
          //params.user_name + ":" + input_text + "\n";
    if (input_text != null) {
      command.params.prompt = base_prompt[current_char] + "\n" + base_log[current_char].join("\n") + "\n" +
        params.user_name + ":" + input_text + "\n";
    } else {
      command.params.prompt = base_prompt[current_char] + "\n" + base_log[current_char].join("\n") + "\n";      
    }
    first_run[current_char] = false;
    
  } else {
    command.cmd = "continue generation";
    command.params = {};
    command.params.char_index = current_char;
    if (input_text != null) {
      command.params.input = log.slice(last_log_index[current_char]).join("\n") + 
                              params.user_name + ":" + input_text + "\n" + 
                              params.char_names[current_char] + ":";
    } else {
      command.params.input = log.slice(last_log_index[current_char]).join("\n") + 
                              params.char_names[current_char] + ":";      
    }
  }
    
  window.command.postMessage(command);
  if (input_text != null)
    log.push(params.user_name + ": " + input_text);
  tmplog = "";
}



function saveSettings() {
  
  // go through the settings and update gpt_params
  let values = document.getElementsByClassName("settings-entry-value");
  let i, j, k, name, changed = false, classes;
    
  // handle general settings
  elem = document.getElementById("settings-general");
  values = elem.getElementsByClassName("settings-entry-value");
  
  for (i = 0; i < values.length; i++) {

    name = values.item(i).classList[1].split("-").at(0);
    
    if (name == "auto_n_keep") { // handle checkboxes separately
      if (params[name] != values.item(i).checked) {
        params[name] = values.item(i).checked;
        changed = true;
      }
      
    } else if (params[name] != values.item(i).value) { 
      params[name] = values.item(i).value;
      changed = true;
    }
  }

  // handle settings of each character
  for (i = 0; i < params.n_chars; i++) {

    elem = document.getElementById("settings-" + i);
    values = elem.getElementsByClassName("settings-entry-value");
   
    for (j = 0; j < values.length; j++) {

      name = values.item(j).classList[1].split("-").at(0);
      
      if (params.gpt_params[i][name] != values.item(j).value) {
          
        if (name == "antiprompt") { // create an array of antiprompt entries

          params.gpt_params[i][name] = [];
          let tmp = values.item(j).value.split("]");
          for (k = 0; k < tmp.length - 1; k++) { // last one is empty element
            // remove [ from the beginning
            params.gpt_params[i][name].push(tmp.at(k).substr(1));
          }
            
        } else if (name == "logit_bias") { // create an array of antiprompt entries
          params.gpt_params[i][name] = [];
          var tmp = values.item(j).value.split("]");
          for (k = 0; k < tmp.length - 1; k++) { // last one is empty element
            let tmp2 = tmp.at(k).split(",");
            params.gpt_params[i][name][k] = [];
            params.gpt_params[i][name][k][0] = tmp2.at(0).substr(1); // remove [ from the beginning
            params.gpt_params[i][name][k][1] = tmp2.at(1);
          }
            
        } else {
          params.gpt_params[i][name] = values.item(j).value;
        }
        changed = true;
      } /* if (params... */
    } /* for (values... */
  } /* for (n_chars... */
  
  statusbar.textContent += ". Settings changed: " + changed;
    
  // if there are changes, send new params to backend
  if (changed) {
    let command = {};
    command.cmd = "set params";
    command.params = {};
    command.params.params = JSON.stringify(params);

    window.command.postMessage(command);
  }
    
  // hide settings popup
  settingsPopup.style.display="none";
}


// Parses prompt and populates base prompt and initial chat messages based on it
function parsePrompt(prompt, char_index) {
  prompt_parsed = true;
  lines = prompt.split("\n");
  let prompt_final_line = -1;
  var i;
  
  for (i = 0; i < lines.length; i++) {
    if (lines.at(i).startsWith(params.char_names[char_index]+":")) { // Char: line found, add to the chat
      if (prompt_final_line == -1)
        prompt_final_line = i - 1;
      appendCharMessage(lines.at(i).substr(params.char_names[char_index].length+1), char_index);
      base_log[char_index].push(lines.at(i)); // add message to the log
      
    } else if (lines.at(i).startsWith(params.user_name+":")) { // same for the User
      if (prompt_final_line == -1)
        prompt_final_line = i - 1;

      // don't add the line if it's only "user_name:" (reverse prompt)
      if (lines.at(i) != (params.user_name+":")) {
        appendUserMessage(lines.at(i).substr(params.user_name.length+1));
        base_log[char_index].push(lines.at(i));
      }
    }
  }
  
  if (prompt_final_line >= 0) {
    base_prompt[char_index] = lines.slice(0, prompt_final_line + 1).join("\n");
  } else {
    base_prompt[char_index] = prompt; // no chat lines found, use whole prompt as base prompt
  }
  
  if (char_index > 0) // add line break before next char's base prompt
    base_prompt_elem.innerHTML += "<br>";
  
  base_prompt_elem.innerHTML += "<b>" + params.char_names[char_index] + "</b>: " +
                                  base_prompt[char_index] + "<br>";
}


// called by LLM
function LLMOutput(token) {
  // update chat log etc.
  tmplog = tmplog + token;
    
  // find last messagebox of AI and modify it's text field.... 
  let last_messagebox = Array.from(document.querySelectorAll('.left-msg')).pop();
  let text_field = last_messagebox.querySelector('.msg-text');
    
  // replace \n in token with <br>
  token = token.replaceAll("\n", "<br>");
    
  text_field.innerHTML = text_field.innerHTML + token;
    
  // check if last part of innerHTML matches with antiprompt + ":" and remove it
  // FIXME: multiple gpt_params
  for (let i = 0; i < params.gpt_params[0].antiprompt.length; i++) {
    if (text_field.innerHTML.endsWith(params.gpt_params[0].antiprompt.at(i))) {
      text_field.innerHTML = text_field.innerHTML.slice(0, -params.gpt_params[0].antiprompt.at(i).length);
      // also remove it from tmplog
      tmplog = tmplog.slice(0, -params.gpt_params[0].antiprompt.at(i).length);
      break;
    }
  }
  
  updateStatusbar(params.char_names[current_char]+" is typing...");
  // check the same for AI name
  if (text_field.innerHTML.endsWith(params.char_names[current_char] + ":"))
    text_field.innerHTML = text_field.innerHTML.slice(0, -(params.char_names[current_char].length + 1));
    
  chat_elem.scrollTop += 500;
}


// sends chatlog (with base prompt) to the backend
function retrieveLog() {
  tmp = {};
  tmp["base_prompt"] = base_prompt;
  tmp["log"] = log;
  return tmp;
}


// Utility functions
function formatDate(date) {
  const hour = "0" + date.getHours();
  const minutes = "0" + date.getMinutes();
  const seconds = "0" + date.getSeconds();

  return `${hour.slice(-2)}:${minutes.slice(-2)}:${seconds.slice(-2)}`;
}

function random(min, max) {
  return Math.floor(Math.random() * (max - min) + min);
}

function openSettingsTab(evt, tab_name) {

  var i, tabs, links;
  
  // Get all elements with class="tabcontent" and hide them
  tabs = document.getElementsByClassName("settings-tab");
  for (i = 0; i < tabs.length; i++) {
    tabs[i].style.display = "none";
  }

  // Get all elements with class="settings-link" and remove the class "active"
  links = document.getElementsByClassName("settings-link");
  for (i = 0; i < links.length; i++) {
    links[i].className = links[i].className.replace(" active", "");
  }

  // Show the current tab, and add an "active" class to the button that opened the tab
  document.getElementById(tab_name).style.display = "block";
  evt.currentTarget.className += " active";
} 

function onLoad() { // called when the UI is reloaded, not used yet

}
