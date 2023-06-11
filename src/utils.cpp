#include "utils.h"

// returns contents of the text file as string
std::string utils::ReadTextFile(std::string path) {
    std::ifstream file(path);
    if (!file.is_open()) // return empty string
        return std::string();
       
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    return buffer.str();
}
    
// writes string to the file, return success status
bool utils::WriteTextFile(std::string contents, std::string path) {
    std::ofstream file(path);
    if (!file.is_open()) { // if file doesn't exist, create it by using append mode
        file.open(path, std::ofstream::app);
        if (!file.is_open()) // can't open or create file
            return false;
    }
            
    file << contents;
    file.close();
    return true;
}

    
// replaces characters that are problematic for javascript
std::string utils::CleanStringForJS(std::string input) {
    std::string output;

    output = std::regex_replace(input, std::regex("\""), "&quot;");
    output = std::regex_replace(output, std::regex("\'"), "&apos;");
    output = std::regex_replace(output, std::regex("<"), "&lt;");
    output = std::regex_replace(output, std::regex(">"), "&gt;");
    output = std::regex_replace(output, std::regex("\n"), "\\n");
        
    return output;
}
   
   
// converts string received from JS to normal format
std::string utils::CleanJSString(std::string input) {
    std::string output;

    output = std::regex_replace(input, std::regex("&quot;"), "\"");
    output = std::regex_replace(output, std::regex("&apos;"), "\'");
    output = std::regex_replace(output, std::regex("&lt;"), "<");
    output = std::regex_replace(output, std::regex("&gt;"), ">");
    output = std::regex_replace(output, std::regex("\\n"), "\n");

    return output;
}
