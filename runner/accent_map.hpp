#pragma once
#include <Windows.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include "logger.hpp"

#define CONFIG_FILE_NAME L"consoleidator_map.txt"

inline std::unordered_map<std::string, wchar_t> defAccentMap = 
{
	{"a'", L'á'},
	{"A'", L'Á'},
	{"a`", L'à'},
	{"A`", L'À'},
	{"a:", L'ä'},
	{"A:", L'Ä'},
	{"a(", L'ă'},
	{"A(", L'Ă'},
	{"a^", L'â'},
	{"A^", L'Â'},
	{"e'", L'é'},
	{"E'", L'É'},
	{"e`", L'è'},
	{"E`", L'È'},
	{"e:", L'ë'},
	{"E:", L'Ë'},
	{"i'", L'í'},
	{"i`", L'ì'},
	{"I`", L'Ì'},
	{"I'", L'Í'},
	{"i^", L'î'},
	{"I^", L'Î'},
	{"i:", L'ï'},
	{"I:", L'Ï'},
	{"ij", L'ĳ'},
	{"IJ", L'Ĳ'},
	{"o'", L'ó'},
	{"O'", L'Ó'},
	{"o:", L'ö'},
	{"O:", L'Ö'},
	{"o\"", L'ő'},
	{"O\"", L'Ő'},
	{"u'", L'ú'},
	{"U'", L'Ú'},
	{"u:", L'ü'},
	{"U:", L'Ü'},
	{"u\"", L'ű'},
	{"U\"", L'Ű'},
	{"s,", L'ș'},
	{"S,", L'Ș'},
	{"ss", L'ß'},
	{"SS", L'ẞ'},
	{"t,", L'ț'},
	{"T,", L'Ț'}
};

inline std::unordered_map<std::string, wchar_t> accentMap;

auto MergeAccentMaps(const std::wstring& workingDirectory) -> int;