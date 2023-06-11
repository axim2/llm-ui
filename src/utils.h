#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <fstream>
#include <regex>
#include <string>

namespace utils {

std::string ReadTextFile(std::string path);
bool WriteTextFile(std::string contents, std::string path);
std::string CleanStringForJS(std::string input);
std::string CleanJSString(std::string input);

}
#endif // UTILS_H
