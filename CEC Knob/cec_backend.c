/*
Copyright © 2026 Leonard Matthew Teyssier

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see https://www.gnu.org/licenses/.
*/
#ifndef UNICODE
#define UNICODE
#endif


#include <windows.h>
#include <cecc.h>
#include <string.h>
#include <stdio.h>
#include <strsafe.h>



#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

//#define WINDOWS_2000 //Uncomment this line to supposedly have compatibility with Windows 2000. However, this is not tested and may not work as expected. Also good luck trying to compile this on Windows 2000 as this is newer than even C99. Also don't get me started with libcec and drivers.


#ifndef WINDOWS_2000
#include <pathcch.h>
#pragma comment(lib, "pathcch.lib")
#endif


extern WCHAR g_CustomDllPath[MAX_PATH];



// 1. Define function pointer types matching the C API signatures
typedef libcec_connection_t(*pfn_libcec_initialise)(libcec_configuration*);
typedef INT(*pfn_libcec_find_adapters)(libcec_connection_t, cec_adapter_descriptor*, INT, CONST CHAR*);
typedef VOID(*pfn_libcec_destroy)(libcec_connection_t);
typedef INT(*pfn_libcec_open)(libcec_connection_t, CONST CHAR*, uint32_t);
typedef VOID(*pfn_libcec_close)(libcec_connection_t);
typedef cec_logical_addresses(*pfn_libcec_get_logical_addresses)(libcec_connection_t);
typedef INT(*pfn_libcec_send_keypress)(libcec_connection_t, cec_logical_address, cec_user_control_code, uint8_t);
typedef INT(*pfn_libcec_send_key_release)(libcec_connection_t, cec_logical_address, uint8_t);

// 2. Declare global function pointers and the DLL handle
static HMODULE hLibCec = NULL;
static pfn_libcec_initialise f_libcec_initialise = NULL;
static pfn_libcec_find_adapters f_libcec_find_adapters = NULL;
static pfn_libcec_destroy f_libcec_destroy = NULL;
static pfn_libcec_open f_libcec_open = NULL;
static pfn_libcec_close f_libcec_close = NULL;
static pfn_libcec_get_logical_addresses f_libcec_get_logical_addresses = NULL;
static pfn_libcec_send_keypress f_libcec_send_keypress = NULL;
static pfn_libcec_send_key_release f_libcec_send_key_release = NULL;

static libcec_connection_t connection = NULL;

// 3. Load the DLL and resolve symbols dynamically
static INT load_libcec_dll(VOID)
{
	//Yes I know this is all sloppy. I hated writing it.
	WCHAR path[MAX_PATH];
	if (g_CustomDllPath[0] != L'\0') StringCchCopyW(path, ARRAYSIZE(path), g_CustomDllPath);

	else StringCchCopyW(path, ARRAYSIZE(path), L"C:\\Program Files\\Pulse-Eight\\USB-CEC Adapter\\cec.dll");

	HMODULE hLibCec = LoadLibraryW(path);


	//This is a fallback in case the dll is not found in the default location. It will try to load it from the same directory as the .exe.
	if (!hLibCec)
	{
		// Get the directory of the running .exe
		GetModuleFileNameW(NULL, path, MAX_PATH);
		//WCHAR* lastSlash = StrRChrW(path, L'\\');
		//if (lastSlash) *(lastSlash + 1) = L'\0'; // Keep the trailing backslash
		PathRemoveFileSpecW(path);
		
		#ifndef WINDOWS_2000
		PathCchAppend(path, _countof(path), L"cec.dll");//
		#else
		lstrcatW(path, L"\\cec.dll");
		#endif

		// Append the DLL name to build the local path
		//StringCchCatW(path, ARRAYSIZE(path), L"cec.dll");

		// Try loading again from next to the .exe
		hLibCec = LoadLibraryW(path);
	}


	//This is the actual error checking.
	if (!hLibCec)
	{
		DWORD errorCode = GetLastError();
		LPWSTR errorText = NULL;


		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errorText, 0, NULL);

		if (errorText)
		{
			WCHAR message[512];
			// 3. Use the cached errorCode here too!
			wsprintfW(message, L"Failed to load cec.dll!\n\nPath:\n%s\n\nError (%lu):\n%s", path, errorCode, errorText);
			MessageBox(NULL, message, L"DLL Load Error", MB_ICONERROR);
			LocalFree(errorText);
		}
		return 0;
	}
		




	//orginals are without the f_ prefix.
	f_libcec_initialise = (pfn_libcec_initialise)GetProcAddress(hLibCec, "libcec_initialise");
	f_libcec_find_adapters = (pfn_libcec_find_adapters)GetProcAddress(hLibCec, "libcec_find_adapters");
	f_libcec_destroy = (pfn_libcec_destroy)GetProcAddress(hLibCec, "libcec_destroy");
	f_libcec_open = (pfn_libcec_open)GetProcAddress(hLibCec, "libcec_open");
	f_libcec_close = (pfn_libcec_close)GetProcAddress(hLibCec, "libcec_close");
	f_libcec_get_logical_addresses = (pfn_libcec_get_logical_addresses)GetProcAddress(hLibCec, "libcec_get_logical_addresses");
	f_libcec_send_keypress = (pfn_libcec_send_keypress)GetProcAddress(hLibCec, "libcec_send_keypress");
	f_libcec_send_key_release = (pfn_libcec_send_key_release)GetProcAddress(hLibCec, "libcec_send_key_release");

	// Verify all functions loaded successfully
	if (!f_libcec_initialise || !f_libcec_find_adapters || !f_libcec_destroy || !f_libcec_open || !f_libcec_close || !f_libcec_get_logical_addresses || !f_libcec_send_keypress || !f_libcec_send_key_release)
	{
		FreeLibrary(hLibCec);
		hLibCec = NULL;
		return 0;
	}
	return 1;
}



//Not using win32 types here as this code is almost portable.
int cec_init(void)
{
	// Attempt to load the DLL at runtime. If it fails, fail gracefully.
	if (!load_libcec_dll()) return -1;

	// Zero the configuration struct
	libcec_configuration config;
	memset(&config, 0, sizeof(config));

	config.clientVersion = LIBCEC_VERSION_CURRENT;
	config.deviceTypes.types[0] = CEC_DEVICE_TYPE_PLAYBACK_DEVICE;
	//config.deviceTypes.count = 1;

	connection = f_libcec_initialise(&config);
	if (!connection) return 1;

	cec_adapter_descriptor adapters[10];
	int found = f_libcec_find_adapters(connection, adapters, 10, NULL);
	if (found <= 0)
	{
		f_libcec_destroy(connection);
		connection = NULL;
		return 2;
	}

	// Open first adapter
	if (!f_libcec_open(connection, adapters[0].strComName, 5000))
	{
		f_libcec_destroy(connection);
		connection = NULL;
		return 3;
	}

	return 0;
}

void cec_shutdown(void)
{
	if (connection)
	{
		f_libcec_close(connection);
		f_libcec_destroy(connection);
		connection = NULL;
	}
}

static void send_key(cec_user_control_code code)
{
	if (!connection) return;
	cec_logical_address primary = f_libcec_get_logical_addresses(connection).primary;
	f_libcec_send_keypress(connection, primary, code, 1);
	Sleep(50);
	f_libcec_send_key_release(connection, primary, 1);
}

void cec_volume_up(void) { send_key(CEC_USER_CONTROL_CODE_VOLUME_UP); }
void cec_volume_down(void) { send_key(CEC_USER_CONTROL_CODE_VOLUME_DOWN); }
void cec_volume_mute(void) { send_key(CEC_USER_CONTROL_CODE_MUTE); }