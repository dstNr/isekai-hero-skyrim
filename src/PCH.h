#pragma once

// CommonLibSSE-NG's headers rely on this being force-included first:
// it defines `namespace stl` and the base setup the other headers use.
#include <SKSE/Impl/PCH.h>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <spdlog/sinks/basic_file_sink.h>

// The auto-generated plugin file (and CommonLibSSE-NG) use "..."sv literals;
// make the literal operators available project-wide via the PCH.
using namespace std::literals;

// Convenience alias used throughout the project: logger::info(...) etc.
namespace logger = SKSE::log;
