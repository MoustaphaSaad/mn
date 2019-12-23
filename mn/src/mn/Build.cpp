#include "mn/Base.cpp"
#include "mn/Context.cpp"
#include "mn/Memory_Stream.cpp"
#include "mn/OS.cpp"
#include "mn/Pool.cpp"
#include "mn/Reader.cpp"
#include "mn/Rune.cpp"
#include "mn/Str.cpp"
#include "mn/Str_Intern.cpp"
#include "mn/Stream.cpp"

// memory
#include "mn/memory/Arena.cpp"
#include "mn/memory/CLib.cpp"
#include "mn/memory/Fast_Leak.cpp"
#include "mn/memory/Leak.cpp"
#include "mn/memory/Stack.cpp"
#include "mn/memory/Virtual.cpp"

// linux
#if defined(OS_LINUX)
#include "mn/linux/Debug.cpp"
#include "mn/linux/File.cpp"
#include "mn/linux/Path.cpp"
#include "mn/linux/Thread.cpp"
#include "mn/linux/Virtual_Memory.cpp"
#elif defined(OS_WINDOWS)
#include "mn/winos/Debug.cpp"
#include "mn/winos/File.cpp"
#include "mn/winos/Path.cpp"
#include "mn/winos/Thread.cpp"
#include "mn/winos/Virtual_Memory.cpp"
#endif

// utf8proc
#include "utf8proc/utf8proc.cpp"
