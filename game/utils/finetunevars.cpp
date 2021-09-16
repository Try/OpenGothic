#include "finetunevars.h"

std::map<std::string, std::string> FinetuneVars::vars;
FinetuneVars* FinetuneVars::instance;

std::string FinetuneVars::filepath;

FinetuneVars& FinetuneVars::inst() {
	return *instance;
}

FinetuneVars::~FinetuneVars() {
	filepath = "C:/Files/repos/OpenGothic/game/utils/finetunevars.ini";
	parseVars();
	instance = this;
}

FinetuneVars::FinetuneVars() {
	instance = nullptr;
}

std::map<std::string, std::string> FinetuneVars::getVars() {
	return vars;
}

void FinetuneVars::parseVars() {

	std::ifstream varsfile(filepath);

	int i = 0;

	std::string line;
	while (std::getline(varsfile, line)) {

		std::stringstream issline(line);
		std::string name, val, dummy;
		std::getline(issline, name, ':');
		std::getline(issline, dummy, ' ');
		std::getline(issline, val);

		vars[name] = val;
	}
}