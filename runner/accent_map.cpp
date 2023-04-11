// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "accent_map.hpp"

auto MergeAccentMaps(const std::wstring& workingDirectory) -> int
{
	// we copy the original map in order to provide a fallback in case the config file is not found or something gets corrupted
	accentMap = defAccentMap;
	std::ifstream configFile(workingDirectory + CONFIG_FILE_NAME, std::ios::binary);

	if (!configFile.is_open())
	{
		logger::LogInfo("No config file present, falling back to built-in accent map", __FILE__, __LINE__);
		// no config file present, return
		return 0;
	}

	logger::LogInfo("Found config file, merging maps", __FILE__, __LINE__);

	std::string line{}, keyBuffer{}, valueBuffer{};
	std::wstring convertedValues{};
	while (configFile>>line)
	{
		for (size_t i = 0; i < line.size(); i++)
		{
			if (line[i] == L'=')
			{
				keyBuffer += line[0];
				keyBuffer += line[1];
				valueBuffer += line.substr(i + 1);
				break;
			}
		}
	}

	// close the file to avoid handle leaks
	configFile.close();

	int result = MultiByteToWideChar(
		CP_UTF8, 
		MB_ERR_INVALID_CHARS, 
		valueBuffer.data(), 
		valueBuffer.size(), 
		nullptr, 
		0
	);

	if (result <= 0)
	{
		std::string error;
		error = "Exception occurred: Failure to convert accent map values buffer size using MultiByteToWideChar: convertResult=";
		error += std::to_string(result);
		error += "  GetLastError()=" + std::to_string(GetLastError());
		logger::LogError(error, __FILE__, __LINE__);
		return -1;
	}

	convertedValues.resize(result + 1);

	result = MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		valueBuffer.data(),
		valueBuffer.size(),
		convertedValues.data(),
		convertedValues.size()
	);

	if (result <= 0)
	{
		std::string error;
		error = "Exception occurred: Failure to convert accent map values using MultiByteToWideChar: convertResult=";
		error += std::to_string(result);
		error += "  GetLastError()=" + std::to_string(GetLastError());
		logger::LogError(error, __FILE__, __LINE__);
		return -1;
	}

	for (size_t i = 0; i < keyBuffer.length(); i += 2)
	{
		accentMap[keyBuffer.substr(i, 2)] = convertedValues[i / 2];
	}

	return 1;
}