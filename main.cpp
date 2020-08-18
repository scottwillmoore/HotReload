#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <filesystem>

using namespace std;

// I/O needs to be disabled.
// Or. I/O needs to be managed, as the library will most-likely want to control I/O.

// It needs to be compiled and tested as a DLL.

// It needs to be version controlled.

// You may want to pair it with an injection method.

// You could have it load the library, run a library defined setup function.
// Recompilation will run a custom library defined load and unload function.
// This allows the entire process to be controlled by the library.
// However, it will add some complexity and how would setup state persist over reload.

// DLL_PROCESS_ATTACH
// if (firstLoad) { setup console (system, or IMGUI)! customize reload behaviour. persistent state? }
// load()
// DLL_PROCESS_DETACH
// free()

// To expand of this idea:
// -> Load HotReload
// -> Load Library
// -> Library calls HotReload, HotReload informs that this is the first execution
// -> Library configures HotReload
// -> E.g. HotReload registers to libraryPath
// -> E.g. Setup console behaviour
// -> E.g. Setup over parameters
// -> Library calls standard onLoad procedures
// -> (Library recompiled)
// -> HotReload reloads library
// -> Library doesn't setup again, instead just runs standard onLoad procedures
// -> Repeat!

// Should we leave the code as is...
// Or modernize the code a little bit, with C++ features.

// LPWSTR	GetSystemError(errorCode)
// HMODULE	CopyAndLoadLibrary(libraryPath, destinationPath)
// BOOL		TryFreeLibrary(libraryHandle)
// BOOL		WaitForFile(filePath)

//BOOL IsRegistered() {
//
//}
//
//BOOL Register() {
//
//}
//
//BOOL Unregister() {
//
//}

LPWSTR GetError(DWORD errorCode = GetLastError())
{
	LPWSTR error = NULL;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorCode, 0, (LPWSTR)&error, 0, NULL);
	return error;
}

HMODULE CopyAndLoadLibrary(filesystem::path libraryPath, filesystem::path destinationPath)
{
	wcout << "Attempt to copy and load library" << endl;

	try
	{
		filesystem::copy_file(libraryPath, destinationPath, filesystem::copy_options::overwrite_existing);
	}
	catch (filesystem::filesystem_error& error)
	{
		wcout << "Failed to copy library" << endl;
		wcout << error.what() << endl;
		return NULL;
	}

	wcout << endl;
	HMODULE libraryHandle = LoadLibraryW(destinationPath.c_str());
	wcout << endl;

	if (libraryHandle == NULL)
	{
		wcout << "Failed to load library" << endl;
		wcout << GetError() << endl;
		return NULL;
	}

	wcout << "Library loaded" << endl;

	return libraryHandle;
}

BOOL WaitForFile(filesystem::path libraryPath)
{
	DWORD attempt = 0;
	DWORD maxAttempt = 5;
	DWORD sleepDuration = 100;

	wcout << "Wait for file access" << endl;

	while (attempt < maxAttempt)
	{
		auto fileHandle =
			CreateFileW(
				libraryPath.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				NULL,
				OPEN_EXISTING,
				NULL,
				NULL);

		if (fileHandle == INVALID_HANDLE_VALUE)
		{
			DWORD errorCode = GetLastError();
			if (errorCode == ERROR_SHARING_VIOLATION)
			{
				wcout << "Cannot access file due to sharing violation" << endl;
				Sleep(sleepDuration);
				sleepDuration *= 2;
				attempt++;
			}
			else
			{
				wcout << "Cannot access file due to an unknown reason" << endl;
				wcout << GetError() << endl;
				return FALSE;
			}
		}
		else
		{
			wcout << "Can access file" << endl;
			CloseHandle(fileHandle);
			return TRUE;
		}
	}

	return FALSE;
}

// New strategies:
// Create a tree of non-recursive directory changes.
// Create a recursive directory changes, but filter the result by folder/extension.

VOID Run(HMODULE moduleHandle, LPWSTR filePath, DWORD bufferSize = 8192)
{
	auto libraryPath = filesystem::absolute(filePath);

	auto libraryName = libraryPath.filename();
	auto libraryDirectory = libraryPath.parent_path();

	auto destinationDirectory = filesystem::temp_directory_path();
	auto destinationPath = destinationDirectory / libraryName;

	auto libraryHandle = CopyAndLoadLibrary(libraryPath, destinationPath);

	auto directoryHandle =
		CreateFileW(
			libraryDirectory.c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS,
			NULL);
	if (directoryHandle == INVALID_HANDLE_VALUE)
	{
		wcout << "Failed to create handle to directory" << endl;
		wcout << GetError() << endl;
		return;
	}

	// auto buffer = make_unique<BYTE[]>(bufferSize);
	BYTE buffer[8192];

	while (true)
	{
		//wcout << "Read directory changes" << endl;

		DWORD bytesReturned;
		BOOL success =
			ReadDirectoryChangesW(
				directoryHandle,
				buffer,
				sizeof(buffer),
				FALSE,
				FILE_NOTIFY_CHANGE_LAST_WRITE,
				&bytesReturned,
				NULL,
				NULL);

		//wcout << "\t" << bytesReturned << endl;

		if (success == FALSE)
		{
			wcout << "Failed to read changes to directory" << endl;
			wcout << GetError() << endl;
			return;
		}

		BYTE* p = buffer;

		//FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.get());

		while (true)
		{
			FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(p);

			auto modifiedName = wstring(info->FileName, info->FileNameLength / 2);
			//wcout << info->Action << "\t" << modifiedName << endl;

			if (libraryName == modifiedName)
			{
				wcout << "Library modified" << endl;

				if (libraryHandle != NULL)
				{
					wcout << "Attempt to free library" << endl;

					wcout << endl;
					BOOL success = FreeLibrary(libraryHandle);
					wcout << endl;

					if (success == FALSE)
					{
						wcout << L"Failed to free library" << endl;
						wcout << GetError() << endl;
						return;
					}
				}

				BOOL success = WaitForFile(libraryPath);

				if (success == TRUE)
				{
					CopyAndLoadLibrary(libraryPath, destinationPath);
				}
			}

			if (info->NextEntryOffset == 0)
			{
				break;
			}
			p += info->NextEntryOffset;
		}
	}
}

int wmain(int argc, wchar_t** argv)
{
	if (argc > 1)
	{
		Run(NULL, argv[1]);
	}
	else
	{
		wcout << "You must provide a path to a library" << endl
			<< endl;
		wcout << "HotReload.exe <libraryPath>" << endl
			<< endl;
	}
}

/*
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Run, NULL, 0, NULL);
		break;

	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
*/
