#include "mn/Process.h"
#include "mn/Defer.h"
#include "mn/File.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#include <objbase.h>
#include <tlhelp32.h>

namespace mn
{
	// API
	Process
	process_id()
	{
		return Process{ GetCurrentProcessId() };
	}

	Process
	process_parent_id()
	{
		DWORD ppid = 0;
		DWORD pid = GetCurrentProcessId();

		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE)
			return Process{};
		mn_defer(CloseHandle(hSnapshot));

		PROCESSENTRY32 pe32;
		ZeroMemory(&pe32, sizeof(pe32));
		pe32.dwSize = sizeof(pe32);

		if (!Process32First(hSnapshot, &pe32))
			return Process{};

		do
		{
			if (pe32.th32ProcessID == pid)
			{
				WCHAR buffer[MAX_PATH];

				HANDLE parent_handle =
					OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (pe32.th32ParentProcessID));
				if (!parent_handle)
					continue;
				mn_defer(CloseHandle(parent_handle));

				if (!GetModuleFileNameEx(parent_handle, 0, buffer, MAX_PATH))
					continue;

				auto path = from_os_encoding(block_from(buffer));
				auto exe_name = from_os_encoding(block_from(pe32.szExeFile));
				mn_defer({
					str_free(path);
					str_free(exe_name);
				});

				if (str_find(path, exe_name, 0) < path.count)
					ppid = pe32.th32ParentProcessID;

				break;
			}
		} while (Process32Next(hSnapshot, &pe32));

		return Process{ ppid };
	}

	bool
	process_kill(Process p)
	{
		auto handle = OpenProcess(PROCESS_ALL_ACCESS, false, DWORD(p.id));
		if (handle == INVALID_HANDLE_VALUE)
			return false;
		mn_defer(CloseHandle(handle));
		return TerminateProcess(handle, 0);
	}

	bool
	process_alive(Process p)
	{
		auto handle = OpenProcess(SYNCHRONIZE, false, DWORD(p.id));
		if (handle == INVALID_HANDLE_VALUE)
			return false;
		mn_defer(CloseHandle(handle));
		auto res = WaitForSingleObject(handle, 0);
		return res == WAIT_TIMEOUT;
	}
}
