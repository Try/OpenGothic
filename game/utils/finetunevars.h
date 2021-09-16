#pragma once

#include <sstream>
#include <fstream>
#include <iostream>
#include <map>

// This class reads variables from a file that need to be finetuned in order for gameplay to feel like the original G2.
// The values are read from a file once per tick, so they can be updated live -> no need for rebuild, restart or reload.
class FinetuneVars {
public:

	static FinetuneVars& inst();

	FinetuneVars();
	~FinetuneVars();

	FinetuneVars(FinetuneVars const&) = delete;
	void operator=(FinetuneVars const&) = delete;

	static std::map<std::string, std::string> getVars();

	static void parseVars();

private:

	static std::map<std::string, std::string> vars;
	static FinetuneVars* instance;

	static std::string filepath;
};