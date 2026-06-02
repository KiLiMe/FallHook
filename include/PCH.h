// AI CONTEXT: Precompiled header for the version-neutral F4SE plugin target.
// Depends on CommonLibF4, F4SE, and commonlib-shared REX logging.
// Runtime assumptions: no owning game version; concrete modules own runtime-specific logic.
// Version-specific logic: none; do not add hook offsets or runtime branches here.
// Source-free policy: shared helpers must not introduce original-text lookup modes.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "RE/Fallout.h"
#include "F4SE/F4SE.h"

#ifdef ERROR
#	undef ERROR
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace std::literals;

#include "Plugin.h"
