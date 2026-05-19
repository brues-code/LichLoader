#pragma once

#include <windows.h>

// Utility function to get the last error message
LPWSTR GetLastErrorMessage();

// Set process DPI awareness using SetProcessDpiAwarenessContext (if available) or SetProcessDPIAware
void EnableDPIAwareness();
