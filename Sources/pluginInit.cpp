#include "3DS.h"
#include "CTRPluginFrameworkImpl/arm11kCommands.h"
#include "CTRPluginFrameworkImpl.hpp"
#include "CTRPluginFramework.hpp"
#include "CTRPluginFrameworkImpl/Graphics/Font.hpp"
#include "csvc.h"

extern "C" void     abort(void);
extern "C" void     initSystem();
extern "C" void     initLib();
extern "C" void     resumeHook(void);
extern "C" Result   __sync_init(void);
extern "C" void     __system_initSyscalls(void);
extern "C" void     CResume(void);
extern "C" Thread   g_mainThread;

u32     g_resumeHookAddress = 0; ///< Used in arm11k.s for resume hook
Thread  g_mainThread = nullptr; ///< Used in syscalls.c for __ctru_get_reent

namespace CTRPluginFramework
{
    void    ThreadExit(void) __attribute__((noreturn));
    static void    Resume(void);
}

void abort(void)
{
    CTRPluginFramework::Color c(255, 69, 0); //red(255, 0, 0);
    CTRPluginFramework::ScreenImpl::Top->Flash(c);
    CTRPluginFramework::ScreenImpl::Bottom->Flash(c);

    CTRPluginFramework::ThreadExit();
}

void CResume(void)
{
    CTRPluginFramework::Resume();
}

namespace CTRPluginFramework
{
    // Threads stacks
    static u32  threadStack[0x4000] ALIGN(8);
    static u32  keepThreadStack[0x1000] ALIGN(8);

    // Some globals
    FS_Archive  _sdmcArchive;
    Handle      g_continueGameEvent = 0;
    Handle      g_keepThreadHandle;
    Handle      g_keepEvent = 0;
    Handle      g_resumeEvent = 0;
    bool        g_keepRunning = true;
    
    static u32      g_backup[2] = {0}; ///< For the resume hook
    extern u32      g_linearOp; ///< allocateHeaps.cpp
    extern bool     g_heapError; ///< allocateHeaps.cpp

    void    ThreadInit(void *arg);

    // From main.cpp
    void    PatchProcess(void);
    int     main(void);

    static void    Resume(void)
    {
        svcSignalEvent(g_resumeEvent);
        *(u32 *)g_resumeHookAddress = g_backup[0];
        *(u32 *)(g_resumeHookAddress + 4) = g_backup[1];
        svcWaitSynchronization(g_resumeEvent, U64_MAX);
        svcCloseHandle(g_resumeEvent);
        g_resumeEvent = 0;
    }

    static u32      InstallResumeHook(void)
    {
        Hook    hook;
        u32     pattern = 0xE59F0014;
        u32     address = 0;

        for (u32 addr = 0x100000; addr < 0x100050; addr += 4)
        {
            if (*(u32 *)addr == pattern)
            {
                address = addr;
                break;
            }
        }

        if (address == 0)
            return (0);

        address -= 0x14;
        g_resumeHookAddress = address;

        g_backup[0] = *(u32 *)address;
        g_backup[1] = *(u32 *)(address + 4);

        hook.Initialize(address, (u32)resumeHook);
        hook.Enable();
        return (address);
    }

    void    KeepThreadMain(void *arg)
    {
        // Initialize the synchronization subsystem
        __sync_init();

        // Initialize newlib's syscalls
        __system_initSyscalls();
        
        // Initialize services
        srvInit();
        acInit();
        amInit();
        fsInit();
        cfguInit();

        // Init Framework's system constants
        SystemImpl::Initialize();

        // Init Process info
        ProcessImpl::Initialize();

        // Init Screen
        ScreenImpl::Initialize();

        // Protect code
        Process::CheckAddress(0x00100000, 7);
        Process::CheckAddress(0x00102000, 7); ///< NTR seems to protect the first 0x1000 bytes, which split the region in two
        // Protect VRAM
        Process::ProtectRegion(0x1F000000, 3);

        // Patch process before it starts
        PatchProcess();

        // Install resume hook
        svcCreateEvent(&g_resumeEvent, RESET_ONESHOT);
        
        if (InstallResumeHook())
        {
            // Continue game
            svcSignalEvent(g_continueGameEvent);

            // Wait until game signal our thread
            svcWaitSynchronization(g_resumeEvent, U64_MAX);
            svcClearEvent(g_resumeEvent);
        }
        else
        {
            // Clean event
            svcCloseHandle(g_resumeEvent);
            g_resumeEvent = 0;

            // Continue game
            svcSignalEvent(g_continueGameEvent);

            // Hook failed, so let's wait 5 seconds for the game to starts
            Sleep(Seconds(5));
        }

        // Correction for some games like Kirby
        //u64 tid = Process::GetTitleID();

        // Pokemon Sun & Moon
     /*   if (tid == 0x0004000000175E00 || tid == 0x0004000000164800
        // ACNL
        ||  tid == 0x0004000000086300 || tid == 0x0004000000086400 || tid == 0x0004000000086200)
            g_linearOp = 0x10203u; */

        // Init heap and newlib's syscalls
        initLib();

        // If heap error, exit
        if (g_heapError)
            goto exit;

        // Create plugin's main thread
        svcCreateEvent(&g_keepEvent, RESET_ONESHOT);
        g_mainThread = threadCreate(ThreadInit, (void *)threadStack, 0x4000, 0x3F, -2, false);

        while (g_keepRunning)
        {
            svcWaitSynchronization(g_keepEvent, U64_MAX); 
            svcClearEvent(g_keepEvent);

            while (ProcessImpl::IsPaused())
            {
                if (ProcessImpl::IsAcquiring())
                    Sleep(Milliseconds(100));
            }
        }

        threadJoin(g_mainThread, U64_MAX);
    exit:
        if (g_resumeEvent)
            svcSignalEvent(g_resumeEvent);
        exit(1);
    }

    void    Initialize(void)
    {
        // Init HID
        hidInit();

        // Init classes
        Font::Initialize();

        // Init event thread
        gspInit();

        //Init OSD
        OSDImpl::_Initialize();

        // Init sdmcArchive
        {
            FS_Path sdmcPath = { PATH_EMPTY, 1, (u8*)"" };
            FSUSER_OpenArchive(&_sdmcArchive, ARCHIVE_SDMC, sdmcPath);
        }
        // Set current working directory
        {
            Directory::ChangeWorkingDirectory(Utils::Format("/plugin/%016llX/", Process::GetTitleID()));
        }

        // Init Process info
        ProcessImpl::UpdateThreadHandle();
    }
    void    InitializeRandomEngine(void);
    // Main thread's start
    void  ThreadInit(void *arg)
    {
        CTRPluginFramework::Initialize();

        // Resume game
        svcSignalEvent(g_resumeEvent);

        // Initialize Globals settings
        InitializeRandomEngine();

        // Start plugin
        int ret = main();

        ThreadExit();
    }

    void  ThreadExit(void)
    {
        if (g_resumeEvent)
            svcSignalEvent(g_resumeEvent);

        // In which thread are we ?
        if (threadGetCurrent() != nullptr)
        {
            // MainThread

            // Release process in case it's currently paused
            ProcessImpl::Play(false);

            // Remove the OSD Hook
            OSDImpl::OSDHook.Disable();

            // Exit services
            gspExit();

            // Exit loop in keep thread
            g_keepRunning = false;
            svcSignalEvent(g_keepEvent);

            threadExit(1);
        }
        else
        {
            // KeepThread
            if (g_mainThread != nullptr)
            {
                ProcessImpl::Play(false);
                PluginMenuImpl::ForceExit();
                threadJoin(g_mainThread, U64_MAX);
            }
            else
                svcSignalEvent(g_continueGameEvent);

            // Exit services
            acExit();
            amExit();
            fsExit();
            cfguExit();            

            exit(-1);
        }
    }

    extern "C" int LaunchMainThread(int arg);
    int   LaunchMainThread(int arg)
    {
        svcCreateEvent(&g_continueGameEvent, RESET_ONESHOT);
        svcCreateThread(&g_keepThreadHandle, KeepThreadMain, 0, &keepThreadStack[0x1000], 0x1A, -2);
        svcWaitSynchronization(g_continueGameEvent, U64_MAX);
        return (0);
    }
}
