#pragma once
#include <Windows.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include "logger.hpp"

#define CONFIG_FILE_NAME L"consoleidator_map.txt"

inline std::unordered_map<std::string, wchar_t> accentMap = 
{
	{"a'", L'á'},
	{"A'", L'Á'},
	{"e'", L'é'},
	{"E'", L'É'},
	{"o'", L'ó'},
	{"O'", L'Ó'},
	{"o\"", L'ő'},
	{"O\"", L'Ő'},
	{"u'", L'ú'},
	{"U'", L'Ú'},
	{"u\"", L'ű'},
	{"U\"", L'Ű'},
	{"i'", L'í'},
	{"I`", L'í'}
};

void InitialiseAccentMap(const std::wstring& workingDirectory);