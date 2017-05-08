#include <3DS.h>
#include "CTRPluginFramework.hpp"
#include "CTRPluginFrameworkImpl/arm11kCommands.h"
#include "CTRPluginFrameworkImpl/System/Screen.hpp"

using namespace CTRPluginFramework;

extern "C" char *fake_heap_start;
extern "C" char *fake_heap_end;
extern "C" u32 __tmp;
extern "C" u32 __ctru_heap;
extern "C" u32 __ctru_heap_size;
extern "C" u32 __ctru_linear_heap;
extern "C" u32 __ctru_linear_heap_size;
extern "C" u32	__linearOp;

bool    g_heapError = false;

extern "C" void __system_allocateHeaps(void);
void __attribute__((weak)) __system_allocateHeaps(void) 
{
    Color red(255, 0, 0);
	
	

	// Allocate the main heap
	__ctru_heap = 0x07500000;
    __ctru_heap_size = 0x100000;    

    if (R_FAILED(arm11kSvcControlMemory(&__ctru_heap, __ctru_heap, __ctru_heap_size, 0x203u, MEMPERM_READ | MEMPERM_WRITE | MEMPERM_EXECUTE)))
    {
        Screen::Top->Flash(red);
        g_heapError = true;
        return;
    }       

    // Allocate the linear heap
    __ctru_linear_heap_size = 0x200000;
    if (R_FAILED(arm11kSvcControlMemory(&__ctru_linear_heap, 0, __ctru_linear_heap_size, __linearOp, MEMPERM_READ | MEMPERM_WRITE | MEMPERM_EXECUTE)))
    {
        Screen::Bottom->Flash(red);
        g_heapError = true;
    }
        

	// Set up newlib heap
	fake_heap_start = reinterpret_cast<char*>(__ctru_heap);
	fake_heap_end = fake_heap_start + __ctru_heap_size;
}