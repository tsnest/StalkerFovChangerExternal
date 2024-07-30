#include <Windows.h>
#include <stdio.h>

#include "resource.h"

HANDLE handle = 0;
LPTHREAD_START_ROUTINE Msg_address = 0;
LPVOID g_fov_address = 0;
FLOAT g_fov = 0.0f;
BOOL isClosed = FALSE;
HANDLE FovChangerThreadHandle;

ATOM RegWindowClass(HINSTANCE, LPCTSTR);

DWORD getModule(LPCSTR module_name) {
	LPVOID module_name_buf = (LPVOID)VirtualAllocEx(handle, NULL, strlen(module_name), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(handle, module_name_buf, module_name, strlen(module_name), NULL);
	LPTHREAD_START_ROUTINE gmh = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetModuleHandleA");
	HANDLE t = CreateRemoteThread(handle, NULL, 0, gmh, module_name_buf, 0, NULL);
	WaitForSingleObject(t, INFINITE);
	DWORD result;
	GetExitCodeThread(t, &result);
	CloseHandle(t);
	VirtualFreeEx(handle, module_name_buf, 0, MEM_RELEASE);
	return result;
}

BOOL ChangeFOV() {
	BOOL isChanged = WriteProcessMemory(handle, g_fov_address, &g_fov, sizeof(float), NULL);
	if (isChanged) {
		char str[10];
		snprintf(str, 10, "FOV: %g", g_fov);

		// Дальше идёт вывод в игровую консоль
		LPVOID msg_buf = (LPVOID)VirtualAllocEx(handle, NULL, strlen(str), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		WriteProcessMemory(handle, msg_buf, str, strlen(str), NULL);
		HANDLE t = CreateRemoteThread(handle, NULL, 0, Msg_address, msg_buf, 0, NULL);
		WaitForSingleObject(t, INFINITE);
		CloseHandle(t);
		VirtualFreeEx(handle, msg_buf, 0, MEM_RELEASE);
	}
	return isChanged;
}

DWORD GetProcessByTitle(LPCSTR title) {
	DWORD pID = 0;
	GetWindowThreadProcessId(FindWindow(NULL, title), &pID);
	return pID;
}

DWORD WINAPI FovChangerThread(HWND hWnd) {
	DWORD game = 0;
	DWORD pID = 0;

	snova:
	if ((pID = GetProcessByTitle("S.T.A.L.K.E.R.: Shadow Of Chernobyl")) == 0) {
		if ((pID = GetProcessByTitle("S.T.A.L.K.E.R.: Clear Sky")) == 0) {
			if ((pID = GetProcessByTitle("S.T.A.L.K.E.R.: Call of Pripyat")) == 0) {
				if (!isClosed) {
					goto snova;
				} else {
					return 0;
				}
			} else {
				game = 3;
			}
		} else {
			game = 2;
		}
	} else {
		game = 1;
	}

	handle = OpenProcess(
		//PROCESS_ALL_ACCESS
		PROCESS_QUERY_INFORMATION | 
		PROCESS_CREATE_THREAD | 
		PROCESS_VM_OPERATION | 
		PROCESS_VM_READ | 
		PROCESS_VM_WRITE, FALSE, pID);
	if (handle) {
		opyat:
		DWORD gameDLL = getModule("xrGame");
		if (gameDLL != 0) {
			povtor:
			DWORD coreDLL = getModule("xrCore");
			if (coreDLL != 0) {
				Msg_address = (LPTHREAD_START_ROUTINE)(coreDLL + (game == 1 ? 0x18640 : game == 2 ? 0x162E0 : 0x15920));
				g_fov_address = (LPVOID)(gameDLL + (game == 1 ? 0x53C598 : game == 2 ? 0x5DC8F8 : 0x635C44));
				ReadProcessMemory(handle, g_fov_address, &g_fov, sizeof(float), NULL);

				if (g_fov == 0.0f) {
					isClosed = TRUE;
					MessageBox(hWnd,
						"Не удалось прочитать значение \"FOV\".\n"
						"Возможно, вы используете неподдерживаемую версию игры или программу блокируют антивирусы.\n\n"
						"Failed to read the \"FOV\" value.\n"
						"Perhaps you are using an unsupported version of the game or the program is blocked by antiviruses."
					, NULL, MB_ICONERROR);
					PostMessage(hWnd, WM_DESTROY, 0, 0);
				} else {
					// Успешно!
					Beep(1000, 200);
				}

				while (!isClosed) {
					DWORD dwExitCode = 0;
					GetExitCodeProcess(handle, &dwExitCode);
					if (dwExitCode != STILL_ACTIVE) {
						isClosed = TRUE;
						MessageBox(hWnd,
							"Процесс игры был закрыт.\n\n"
							"The game process was closed."
						, "...", 0);
						PostMessage(hWnd, WM_DESTROY, 0, 0);
					} else {
						char* err =
							"Не удалось записать значение \"FOV\".\n"
							"Возможно, вы используете неподдерживаемую версию игры или программу блокируют антивирусы или процесс игры был закрыт.\n\n"
							"Failed to write \"FOV\" value.\n"
							"Perhaps you are using an unsupported version of the game or the program is blocked by antiviruses or the game process has been closed."
							;
						if (GetAsyncKeyState(VK_OEM_PLUS) && g_fov < 90.0f) {
							g_fov = g_fov + 0.5f;
							if (!ChangeFOV()) {
								isClosed = TRUE;
								MessageBox(hWnd, err, NULL, MB_ICONERROR);
								PostMessage(hWnd, WM_DESTROY, 0, 0);
							}
						} else if (GetAsyncKeyState(VK_OEM_MINUS) && g_fov >(game == 3 ? 55.0f : 67.5f)) {
							g_fov = g_fov - 0.5f;
							if (!ChangeFOV()) {
								isClosed = TRUE;
								MessageBox(hWnd, err, NULL, MB_ICONERROR);
								PostMessage(hWnd, WM_DESTROY, 0, 0);
							}
						}

						if (!isClosed) {
							Sleep(100);
						}
					}
				}
			} else {
				if (!isClosed) {
					goto povtor;
				}
			}
		} else {
			if (!isClosed) {
				goto opyat;
			}
		}
	} else {
		MessageBox(hWnd,
			"Не удалось открыть процесс игры.\n"
			"Попробуйте отключить антивирусы и перезапустить программу от имени администратора.\n\n"
			"Failed to open the game process.\n"
			"Try disabling antivirus software and restarting the program as administrator."
		, NULL, MB_ICONERROR);
		PostMessage(hWnd, WM_DESTROY, 0, 0);
	}

	if (handle) {
		CloseHandle(handle);
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int width = 552; // Ширина окна
	int height = 616; // Высота окна
	LPCTSTR lpzClass = "StalkerFovChanger";

	if (!RegWindowClass(hInstance, lpzClass)) {
		MessageBox(NULL, "Не удалось создать класс окна!\n\nFailed to create window class!", lpzClass, MB_ICONERROR);
		return 0;
	}

	RECT rect;
	GetWindowRect(GetDesktopWindow(), &rect);
	int x = rect.left + (rect.right - rect.left - width) / 2;
	int y = rect.top + (rect.bottom - rect.top - height) / 2;

	// Создаём окно
	HWND hWnd = CreateWindow(lpzClass, "STALKER MP FOV-CHANGER | " __DATE__, WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_SIZEBOX) | WS_VISIBLE, x, y, width, height, 0, 0, hInstance, 0);
	if (!hWnd) {
		MessageBox(NULL, "Не удалось создать окно!\n\nCould not create window!", lpzClass, MB_ICONERROR);
		return 0;
	}

	// Создаём текст
	HWND hLabel = CreateWindow("static",
		" [Goo.su/ap-pro-fov]\n\n"

		" [RU]\n"
		" Редактор игрового угла обзора STALKER, работающий в мультиплеере.\n\n"

		" Поддерживаемые версии игры:\n"
		"  - Тень Чернобыля [1.0006]\n"
		"  - Чистое Небо [1.5.10]\n"
		"  - Зов Припяти [1.6.0.2]\n\n"

		" Использование:\n"
		"  - Оставляете программу запущенной.\n"
		"  - Запускаете игру.\n"
		"  - Если всё успешно, вы услышите бип при запуске игры :)\n"
		"  - Увеличиваете или уменьшаете FOV клавишами + или - (можно прямо в игре).\n"
		"  - Изменённый FOV будет выведен в консоль игры.\n"
		"  - Не закрывайте программу, пока работает игра.\n\n"

		"================================================\n"
		"================================================\n\n"

		" [EN]\n"
		" STALKER FOV Changer for multiplayer.\n\n"

		" Supported Game Versions:\n"
		"  - Shadow of Chernobyl [1.0006]\n"
		"  - Clear Sky [1.5.10]\n"
		"  - Call of Pripyat [1.6.0.2]\n\n"

		" Using:\n"
		"  - Leave the program running.\n"
		"  - Run the game.\n"
		"  - If everything is successful, you will hear a beep when the game starts :)\n"
		"  - Increase or decrease FOV with the + or - keys (right in the game).\n"
		"  - The changed FOV will be displayed in the game console.\n"
		"  - Do not close the program while the game is running."
	, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, width, height, hWnd, 0, hInstance, 0);

	FovChangerThreadHandle = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)FovChangerThread, hWnd, NULL, NULL);

	MSG msg = {0};
	int iGetOk = 0;
	while ((iGetOk = GetMessage(&msg, 0, 0, 0)) != 0) {
		if (iGetOk == -1) {
			MessageBox(hWnd, "Ошибка обработки окна!\n\nWindow processing error!", 0, MB_ICONERROR);
			return 0;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_DESTROY: {
		isClosed = TRUE;
		WaitForSingleObject(FovChangerThreadHandle, INFINITE);
		CloseHandle(FovChangerThreadHandle);

		PostQuitMessage(0);
		break;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

ATOM RegWindowClass(HINSTANCE hInst, LPCTSTR lpzClassName)
{
	WNDCLASS wcWindowClass = { 0 };
	wcWindowClass.lpfnWndProc = (WNDPROC)WndProc;
	wcWindowClass.style = CS_HREDRAW | CS_VREDRAW;
	wcWindowClass.hInstance = hInst;
	wcWindowClass.lpszClassName = lpzClassName;
	wcWindowClass.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
	wcWindowClass.hCursor = LoadCursor(0, IDC_ARROW);
	wcWindowClass.hbrBackground = (HBRUSH)COLOR_APPWORKSPACE;
	return RegisterClass(&wcWindowClass);
}