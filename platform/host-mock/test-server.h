#pragma once
#include <string>

void startTestServer(int port, const std::string& mockDataFile = "mock-data.txt", double timeScale = 1.0, const std::string& logFile = "", const std::string& logVerbosity = "");