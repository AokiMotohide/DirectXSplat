#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace directxsplat::io {

std::string ToLower(std::string s);
std::string Trim(const std::string& s);
std::vector<std::string> Split(const std::string& s, char delimiter);
bool EndsWithCaseInsensitive(std::string_view value, std::string_view suffix);

}

