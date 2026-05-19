#pragma once

#include <windows.h>

// Read from a UTF-8 text file, optionally ignoring comments
LPWSTR* FromTextFile(LPCWSTR pFileName, int* nLinesRead, BOOL ignoreComments);
