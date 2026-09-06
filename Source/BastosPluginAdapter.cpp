#include "BastosPluginAdapter.h"
#include <cstring>

START_NAMESPACE_DISTRHO
Plugin* createPlugin() { return new BastosPluginAdapter(); }
END_NAMESPACE_DISTRHO
