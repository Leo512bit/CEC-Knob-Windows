/*
©2026 Leonard Matthew Teyssier

This source file is triple licensed with the following options:

-The GNU General Public License version 3 (or later if you so choose).
-A custom permissive license based on the BSD 3-clause license that is more permissive as it excludes the binary clause (in fact in some ways it is more permissive than the BSD 2-clause).
-The University of Illinois/NCSA Open Source License license if you do not want to deal with a custom license.

The custom permissive license is as follows:


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

	(1) Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.

	(2) The name of the author may not be used to
	endorse or promote products derived from this software without
	specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/




#define STRICT


#ifndef UNICODE
#define UNICODE
#endif


#include <windows.h>
#include <winuser.h>


#include <shellapi.h>
#include <strsafe.h>
#define WM_TRAYICON     (WM_APP + 1)
#define ID_TRAY_EXIT    1001




static NOTIFYICONDATA nid;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wp, LPARAM lp);

HHOOK g_hook = NULL;

//LRESULT CALLBACK LowLevelKeyboardProc(INT nCode, WPARAM wParam, LPARAM lParam);



#define WM_APP_VOLUME_UP     (WM_APP + 10)
#define WM_APP_VOLUME_DOWN   (WM_APP + 11)
#define WM_APP_VOLUME_MUTE   (WM_APP + 12)
#define WM_APP_CEC_QUIT      (WM_APP + 13)   // custom quit for worker


static DWORD g_dwWorkerThreadId = 0;
static HANDLE g_hWorkerThread = NULL;
static HANDLE g_hWorkerReady = NULL;   // event to signal worker is ready


static UINT s_uTaskbarCreatedMsg = 0;

WCHAR g_CustomDllPath[MAX_PATH] = L"";


VOID InitNotifyIcon(HWND hwnd)
{
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwnd;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), L"CEC Volume Control");

	Shell_NotifyIcon(NIM_ADD, &nid);
}


DWORD WINAPI CecWorkerThread(LPVOID lpParam)
{

	extern int cec_init(void);

	switch (cec_init())
	{
	case 0:
		break; // Success! Fall through to the rest of the thread logic
	case 2:
		MessageBoxW(NULL, L"Failed to initialize CEC!\nDevice not found.", L"Error", MB_ICONERROR);
		return 1;
	default: // Catches any other non-zero error code
		MessageBoxW(NULL, L"Failed to initialize CEC!", L"Error", MB_ICONERROR);
		return 1;
	}


	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
	// Signal that this thread is ready to receive messages
	SetEvent(g_hWorkerReady);

	//extern void stuff is to avoid needing a header file for the cec functions. Declereations are in block scope to limit function visibility.
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		switch (msg.message)
		{
		case WM_APP_VOLUME_UP:
		{
			extern void cec_volume_up(void);
			cec_volume_up();

			break;
		}
		case WM_APP_VOLUME_DOWN:
		{
			extern void cec_volume_down(void);
			cec_volume_down();

			break;
		}
		case WM_APP_VOLUME_MUTE:
		{
			extern void cec_volume_mute(void);
			cec_volume_mute();

			break;
		}
		case WM_APP_CEC_QUIT:
		{
			extern void cec_shutdown(void);
			cec_shutdown();

			PostQuitMessage(0);
		}	break;
		}
	}

	return 0;
}


LRESULT CALLBACK LowLevelKeyboardProc(INT nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
		if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
		{
			switch (p->vkCode)
			{
			case VK_VOLUME_UP:
				PostThreadMessage(g_dwWorkerThreadId, WM_APP_VOLUME_UP, 0, 0);
				return 1;

			case VK_VOLUME_DOWN:
				PostThreadMessage(g_dwWorkerThreadId, WM_APP_VOLUME_DOWN, 0, 0);
				return 1;

			case VK_VOLUME_MUTE:
				PostThreadMessage(g_dwWorkerThreadId, WM_APP_VOLUME_MUTE, 0, 0);
				return 1;
			}
		}
	}
	return CallNextHookEx(g_hook, nCode, wParam, lParam);
}




VOID ParseDllArgument(VOID)
{
	INT argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv != NULL)
	{
		if (argc > 1 && argv[1] != NULL && argv[1][0] != L'\0') StringCchCopyW(g_CustomDllPath, ARRAYSIZE(g_CustomDllPath), argv[1]);

		LocalFree(argv);
	}
}






INT WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR lpCmdLine, _In_ INT nCmdShow)
{

	ParseDllArgument();
	s_uTaskbarCreatedMsg = RegisterWindowMessage(L"TaskbarCreated");



	// Create ready event
	g_hWorkerReady = CreateEvent(NULL, TRUE, FALSE, NULL);// manual-reset, initially non-signaled

	if (!g_hWorkerReady) return 1;


	g_hWorkerThread = CreateThread(NULL, 0, CecWorkerThread, NULL, 0, &g_dwWorkerThreadId);
	if (!g_hWorkerThread)
	{
		CloseHandle(g_hWorkerReady);
		return 1;
	}


	DWORD waitResult = WaitForSingleObject(g_hWorkerReady, 5000);
	if (waitResult != WAIT_OBJECT_0)
	{
		// Worker failed to start – handle error
		CloseHandle(g_hWorkerThread);
		CloseHandle(g_hWorkerReady);
		return 1;
	}



	WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"TrayAppClass";
	if (!RegisterClassEx(&wc)) goto fail;



	HWND hwnd = CreateWindowEx(0, L"TrayAppClass", L"TrayAppHiddenWindow", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);


	if (!hwnd) goto fail;



	g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
	if (!g_hook)
	{
		MessageBox(NULL, L"Failed to launch keyboard hook!", L"Error", MB_ICONERROR);
		// Clean up worker
	fail:
		PostThreadMessage(g_dwWorkerThreadId, WM_APP_CEC_QUIT, 0, 0);
		WaitForSingleObject(g_hWorkerThread, INFINITE);
		CloseHandle(g_hWorkerThread);
		CloseHandle(g_hWorkerReady);
		return 1;
	}



	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}


	UnhookWindowsHookEx(g_hook);

	// Tell worker to quit and wait for it
	PostThreadMessage(g_dwWorkerThreadId, WM_APP_CEC_QUIT, 0, 0);
	WaitForSingleObject(g_hWorkerThread, INFINITE);

	CloseHandle(g_hWorkerThread);
	CloseHandle(g_hWorkerReady);



	return (INT)msg.wParam;
	//ExitProcess(0);
	//return 0;
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wp, LPARAM lp)
{

	if (uMsg == s_uTaskbarCreatedMsg)
	{
		InitNotifyIcon(hwnd);
		return 0;
	}

	switch (uMsg)
	{
	case WM_CREATE:
	{
		InitNotifyIcon(hwnd);
		break;
	}



	case WM_TRAYICON:
	{
		// Handle mouse interactions on the tray icon
		if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_LBUTTONUP)
		{
			POINT pt;
			GetCursorPos(&pt);


			// Set foreground window so the context menu behaves correctly when clicking away
			SetForegroundWindow(hwnd);

			HMENU hMenu = CreatePopupMenu();
			if (hMenu)
			{
				AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

				// TrackPopupMenu requires the window to be in the foreground
				TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
				DestroyMenu(hMenu);
			}
		}
		break;
	}



	case WM_COMMAND:
	{

		if (LOWORD(wp) == ID_TRAY_EXIT) DestroyWindow(hwnd);
		break;
	}

	case WM_DESTROY:
	{
		Shell_NotifyIcon(NIM_DELETE, &nid);
		PostQuitMessage(0);
		break;
	}

	default:
		return DefWindowProc(hwnd, uMsg, wp, lp);
	}
	return 0;
}