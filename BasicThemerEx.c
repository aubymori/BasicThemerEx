#include <windows.h>
#include <shlobj_core.h>
#include <dwmapi.h>
#include "BasicThemerEx.h"

HINSTANCE hAppInstance;
HANDLE hMutex;
BOOL bBasicEnabled = FALSE;
WCHAR szBlacklistPath[MAX_PATH];
LPWSTR *szBlacklist;

BOOL BeforeSeparator(
	LPWSTR szSource,
	WCHAR separator,
	LPWSTR out
)
{
	if (szSource == NULL || out == NULL)
	{
		return FALSE; // Invalid input
	}

	int nLen = wcslen(szSource);
	int nIndex = -1;

	// Find the last instance of the separator
	for (int i = nLen - 1; i >= 0; i--)
	{
		if (szSource[i] == separator)
		{
			nIndex = i;
			break;
		}
	}

	if (nIndex == -1)
	{
		return FALSE;
	}

	wcsncpy(out, szSource, nIndex);
	out[nIndex] = L'\0';

	return TRUE;
}

#pragma region "Actually enables basic theme"
inline void EnableBasicTheme(HWND hWnd, BOOL bEnable)
{
	DWORD dwNcrpPolicy = bEnable ? DWMNCRP_DISABLED : DWMNCRP_ENABLED;
	DwmSetWindowAttribute(hWnd, DWMWA_NCRENDERING_POLICY, &dwNcrpPolicy, 4);
}

BOOL CALLBACK UpdateBasicEnumProc(
	HWND   hWnd,
	LPARAM lParam
)
{
	EnableBasicTheme(hWnd, (BOOL)lParam);
	return TRUE;
}

/* Enables/disables basic theme on every window currently open. */
inline void UpdateBasicTheme(void)
{
	EnumWindows(UpdateBasicEnumProc, (LPARAM)bBasicEnabled);
}
#pragma endregion

#pragma region "Dialog procedures"
BOOL AboutDlgProc(
	HWND   hWnd,
	UINT   uMsg,
	WPARAM wParam,
	LPARAM lparam
)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			// Correct icon size because dialog units fucking suck ass
			HWND hwIcon = GetDlgItem(hWnd, IDD_ABOUT_ICON);
			SetWindowPos(
				hwIcon, NULL,
				16, 16,
				32, 32, 0
			);
			break;
		}
		case WM_CLOSE:
			EndDialog(hWnd, 0);
			break;
		case WM_COMMAND:
			switch (wParam)
			{
				case IDD_ABOUT_OK:
					EndDialog(hWnd, 0);
					break;
			}
			break;
		default:
			return FALSE;
			break;
	}

	return TRUE;
}

BOOL DlgProc(
	HWND   hWnd,
	UINT   uMsg,
	WPARAM wParam, 
	LPARAM lParam
)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			/* Set dialog icon */
			HICON hIcon = (HICON)LoadImageW(
				hAppInstance,
				MAKEINTRESOURCEW(IDI_BTEX),
				IMAGE_ICON,
				GetSystemMetrics(SM_CXSMICON),
				GetSystemMetrics(SM_CYSMICON),
				0
			);

			if (hIcon)
			{
				SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			}
			break;
		}
		case WM_COMMAND:
			switch (wParam)
			{
				case IDD_BTEX_ABOUT:
					DialogBoxW(hAppInstance, MAKEINTRESOURCEW(IDD_ABOUT), hWnd, AboutDlgProc);
					break;
				case IDD_BTEX_EDIT:
				{
					HANDLE hBlacklist = CreateFileW(
						szBlacklistPath, GENERIC_WRITE,
						0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL
					);

					if (hBlacklist == INVALID_HANDLE_VALUE)
					{
						if (GetLastError() != ERROR_FILE_EXISTS)
						{
							MessageBoxW(
								hWnd,
								L"Failed to create blacklist file.",
								L"BasicThemerEx",
								MB_ICONERROR
							);
							break;
						}
					}
					else
					{
						WCHAR szBlacklistInit[256] = BLACKLIST_INIT;
						BOOL bResult = WriteFile(
							hBlacklist, szBlacklistInit, wcslen(szBlacklistInit) * sizeof(WCHAR), 0, 0
						);
						if (!bResult)
						{
							MessageBoxW(
								hWnd,
								L"Failed to write to blacklist file.",
								L"BasicThemerEx",
								MB_ICONERROR
							);
							CloseHandle(hBlacklist);
							break;
						}
						CloseHandle(hBlacklist);
					}

					ShellExecuteW(
						NULL, NULL,
						L"notepad", szBlacklistPath,
						NULL, SW_SHOWNORMAL
					);
					break;
				}
				case IDD_BTEX_ENABLE:
					bBasicEnabled = !bBasicEnabled;
					SendMessageW(
						GetDlgItem(hWnd, IDD_BTEX_ENABLE),
						BM_SETCHECK,
						bBasicEnabled ? BST_CHECKED : BST_UNCHECKED,
						NULL
					);
					UpdateBasicTheme();
					break;
			}
			break;
		case WM_CLOSE:
			DestroyWindow(hWnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return FALSE;
			break;
	}

	return TRUE;
}
#pragma endregion

LRESULT CALLBACK ShellProc(
	int    nCode,
	WPARAM wParam,
	LPARAM lParam
)
{
	if (nCode == HSHELL_WINDOWCREATED)
	{
		UpdateBasicTheme();
	}

	return 0;
}

int WINAPI wWinMain(
	_In_     HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_     LPWSTR    lpCmdLine,
	_In_     int       nCmdShow
)
{
	/* Determine if BasicThemerEx is already running */
	hMutex = OpenMutexW(
		MUTEX_ALL_ACCESS,
		FALSE,
		L"BasicThemerEx"
	);

	if (!hMutex)
	{
		hMutex = CreateMutexW(
			0,
			0,
			L"BasicThemerEx"
		);
	}
	else
	{
		WCHAR szMessage[256];
		LoadStringW(hAppInstance, IDS_ONEINSTANCE, szMessage, 256);
		MessageBoxW(
			NULL,
			szMessage,
			L"BasicThemerEx",
			MB_ICONWARNING
		);
		return 0;
	}

	if (!IsUserAnAdmin())
	{
		WCHAR szMessage[256];
		LoadStringW(hAppInstance, IDS_NONADMIN, szMessage, 256);
		MessageBoxW(NULL, szMessage, L"BasicThemerEx", MB_ICONWARNING);
	}

	/* Get path EXE is contained in (for blacklist) */
	WCHAR szExePath[MAX_PATH];
	GetModuleFileNameW(
		NULL,
		szExePath,
		MAX_PATH
	);
	if (BeforeSeparator(szExePath, L'\\', szBlacklistPath))
	{
		wcscat_s(szBlacklistPath, MAX_PATH, L"\\BLACKLIST");
	}
	else
	{
		MessageBoxW(NULL, L"Failed to get the folder the executable is contained in.", L"BasicThemerEx", MB_ICONERROR);
		return 1;
	}

	/* Globalize hInstance */
	hAppInstance = hInstance;

	SetWindowsHookExW(
		WH_SHELL,
		ShellProc,
		NULL,
		0
	);

	/* Open dialog box */
	DialogBoxW(hInstance, MAKEINTRESOURCE(100), NULL, DlgProc);

	bBasicEnabled = FALSE;
	UpdateBasicTheme();
	ReleaseMutex(hMutex);
	return 0;
}