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
extern "C" Thread   g_mainThread;

u32     g_resumeHookAddress = 0; ///< Used in arm11k.s for resume hook
Thread  g_mainThread = nullptr; ///< Used in syscalls.c for __ctru_get_reent

namespace CTRPluginFramework
{
    void    ThreadExit(void);
}

void abort(void)
{
    CTRPluginFramework::Color c(255, 69, 0); //red(255, 0, 0);
    CTRPluginFramework::ScreenImpl::Top->Flash(c);
    CTRPluginFramework::ScreenImpl::Bottom->Flash(c);

    CTRPluginFramework::ThreadExit();
    while (true);
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
    
    extern bool     g_heapError; ///< allocateHeaps.cpp

    void    ThreadInit(void *arg);
    void    InstallOSD(void); ///< OSDImpl

    // From main.cpp
    void    PatchProcess(FwkSettings &settings);
    int     main(void);

    static bool     IsPokemonSunOrMoon(void)
    {
        u32 lowtid = (u32)Process::GetTitleID();

        return (lowtid == 0x00164800 || lowtid == 0x00175E00);
    }
    void    KeepThreadMain(void *arg)
    {
        FwkSettings settings;

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

        // Init sdmcArchive
        {
            FS_Path sdmcPath = { PATH_EMPTY, 1, (u8*)"" };
            FSUSER_OpenArchive(&_sdmcArchive, ARCHIVE_SDMC, sdmcPath);
        }

        // Protect code
        Process::CheckAddress(0x00100000, 7);
        Process::CheckAddress(0x00102000, 7); ///< NTR seems to protect the first 0x1000 bytes, which split the region in two
        // Protect VRAM
        Process::ProtectRegion(0x1F000000, 3);

        // Check loader
        SystemImpl::IsLoaderNTR = Process::CheckAddress(0x06000000, 3);

        // Init default settings
        settings.HeapSize = SystemImpl::IsLoaderNTR ? 0x100000 : 0x200000;
        settings.EcoMemoryMode = false;
        settings.WaitTimeToBoot = Seconds(5.f);
        settings.MenuSelectedItemColor = settings.BackgroundBorderColor = settings.WindowTitleColor = settings.MainTextColor = Color(255, 255, 255);
        settings.MenuUnselectedItemColor = Color(160, 160, 160);
        settings.BackgroundMainColor = Color();
        settings.BackgroundSecondaryColor = Color(15, 15, 15);
        settings.CursorFadeValue = 0.2f;

        // Patch process before it starts & let the dev init some settings
        PatchProcess(settings);

        // Continue game
        svcSignalEvent(g_continueGameEvent);

        // Wait for the game to be fully launched
        Sleep(settings.WaitTimeToBoot);

        // Init heap and newlib's syscalls
        initLib();

        // Copy FwkSettings to the globals (solve initialization issues)
        Preferences::Settings = settings;

        // If heap error, exit
        if (g_heapError)
            goto exit;

        // Create plugin's main thread
        svcCreateEvent(&g_keepEvent, RESET_ONESHOT);
        g_mainThread = threadCreate(ThreadInit, (void *)threadStack, 0x4000, 0x18, -2, false);

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
        svcCloseHandle(g_keepEvent);
       /* if (g_resumeEvent)
            svcSignalEvent(g_resumeEvent);*/
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
        Initialize();

        // Resume game
        /*if (g_resumeEvent)
        {
            svcSignalEvent(g_resumeEvent);
            Sleep(Seconds(5.f));
        }   */

        // Initialize Globals settings
        InitializeRandomEngine();

        // Start plugin
        int ret = main();

        ThreadExit();
    }

    void  ThreadExit(void)
    {
     /*   if (g_resumeEvent)
            svcSignalEvent(g_resumeEvent); */

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
