#include "mn/File.h"
#include "mn/Defer.h"
#include "mn/Memory.h"
#include "mn/Thread.h"
#include "mn/Scope.h"
#include "mn/Window.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mbstring.h>
#include <tchar.h>

namespace mn
{
	struct window_data
	{
		uint64_t pid;
		mn::Buf<uint64_t> hwnd;

	};

	inline static bool
	_is_main_window(HWND hwnd)
	{
		return GetWindow(hwnd, GW_OWNER) == (HWND)0 && IsWindowVisible(hwnd);
	}

	inline static BOOL CALLBACK
	_enumerate_windows_callback(HWND hwnd, LPARAM lParam)
	{
		DWORD lpdwProcessId;
		GetWindowThreadProcessId(hwnd, &lpdwProcessId);
		if (lpdwProcessId == ((window_data *)lParam)->pid && _is_main_window(hwnd))
		{
			mn::buf_push(((window_data *)lParam)->hwnd, hwnd);
		}
		return TRUE;
	}

	mn::Buf<uint64_t>
	get_windows_from_pid(uint64_t pid)
	{
		auto buf  = mn::buf_with_capacity<uint64_t>(20);
		window_data data{pid, buf};
		EnumWindows(_enumerate_windows_callback, (LPARAM) &data);
		return data.hwnd;
	}
}