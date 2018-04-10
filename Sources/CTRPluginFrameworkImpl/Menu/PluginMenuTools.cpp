#include "CTRPluginFrameworkImpl/Menu/HotkeysModifier.hpp"
#include "CTRPluginFrameworkImpl/Menu/PluginMenuTools.hpp"
#include "CTRPluginFrameworkImpl/Menu/MenuEntryTools.hpp"
#include "CTRPluginFrameworkImpl/Preferences.hpp"
#include "CTRPluginFramework/Menu/MessageBox.hpp"
#include "CTRPluginFramework/Utils/StringExtensions.hpp"
#include "Hook.hpp"
#include "CTRPluginFramework/System/Sleep.hpp"
#include "CTRPluginFramework/System/Process.hpp"
#include "CTRPluginFramework/Graphics/OSD.hpp"
#include <ctime>
#include <cstring>
#include <cstdio>
#include "ctrulib/srv.h"
#include "ctrulib/util/utf.h"
#include "CTRPluginFramework/Utils/Utils.hpp"

#define ALPHA 1

#if ALPHA
#define VersionStr "CTRPluginFramework Alpha V.0.4.2"
#else
#define VersionStr "CTRPluginFramework Beta V.0.4.0"
#endif

namespace CTRPluginFramework
{
    enum Mode
    {
        NORMAL = 0,
        ABOUT,
        HEXEDITOR,
        GWRAMDUMP,
        MISCELLANEOUS,
        SETTINGS
    };

    static int  g_mode = NORMAL;

    PluginMenuTools::PluginMenuTools(std::string &about, HexEditor &hexEditor) :
        _about(about),
        _mainMenu("Tools"),
        _miscellaneousMenu("Miscellaneous"),
        _settingsMenu("Settings"),
        _hexEditorEntry(nullptr),
        _hexEditor(hexEditor),
        _menu(&_mainMenu, nullptr),
        _abouttb("About", _about, IntRect(30, 20, 340, 200)),
        _exit(false)
    {
        InitMenu();
    }

    static void    MenuHotkeyModifier(void)
    {
        u32 keys = Preferences::MenuHotkeys;

        (HotkeysModifier(keys, "Select the hotkeys you'd like to use to open the menu."))();

        if (keys != 0)
            Preferences::MenuHotkeys = keys;
    }

    void    PluginMenuTools::UpdateSettings(void)
    {
        if (Preferences::UseFloatingBtn) _settingsMenu[1]->AsMenuEntryImpl().Enable();
        else _settingsMenu[1]->AsMenuEntryImpl().Disable();

        if (Preferences::AutoSaveCheats) _settingsMenu[2]->AsMenuEntryImpl().Enable();
        else _settingsMenu[2]->AsMenuEntryImpl().Disable();

        if (Preferences::AutoSaveFavorites) _settingsMenu[3]->AsMenuEntryImpl().Enable();
        else _settingsMenu[3]->AsMenuEntryImpl().Disable();

        if (Preferences::AutoLoadCheats) _settingsMenu[4]->AsMenuEntryImpl().Enable();
        else _settingsMenu[4]->AsMenuEntryImpl().Disable();

        if (Preferences::AutoLoadFavorites) _settingsMenu[5]->AsMenuEntryImpl().Enable();
        else _settingsMenu[5]->AsMenuEntryImpl().Disable();

        if (Preferences::DrawTouchCursor) _miscellaneousMenu[2]->AsMenuEntryTools().Enable();
        else _miscellaneousMenu[2]->AsMenuEntryTools().Disable();

        if (Preferences::ShowTopFps) _miscellaneousMenu[3]->AsMenuEntryTools().Enable();
        else _miscellaneousMenu[3]->AsMenuEntryTools().Disable();

        if (Preferences::ShowBottomFps) _miscellaneousMenu[4]->AsMenuEntryTools().Enable();
        else _miscellaneousMenu[4]->AsMenuEntryTools().Disable();
    }

    using   FsTryOpenFileType = u32(*)(u32, u16*, u32);

    enum HookFilesMode
    {
        NONE = 0,
        OSD = 1,
        FILE = 2
    };
    static Hook         g_FsTryOpenFileHook;
    static u32          g_HookMode = NONE;
    static u32          g_returncode[4];
    static File         g_hookExportFile;
    u32                 g_FsTryOpenFileAddress = 0;
    static LightLock    g_OpenFileLock;

    static u32      FindNearestSTMFD(u32 addr)
    {
        for (u32 i = 0; i < 1024; i++)
        {
            addr -= 4;
            if (*(u16 *)(addr + 2) == 0xE92D)
                return addr;
        }
        return (0);
    }

    static void      FindFunction(u32 &FsTryOpenFile)
    {
        const u8 tryOpenFilePat1[] = { 0x0D, 0x10, 0xA0, 0xE1, 0x00, 0xC0, 0x90, 0xE5, 0x04, 0x00, 0xA0, 0xE1, 0x3C, 0xFF, 0x2F, 0xE1 };
        const u8 tryOpenFilePat2[] = { 0x10, 0x10, 0x8D, 0xE2, 0x00, 0xC0, 0x90, 0xE5, 0x05, 0x00, 0xA0, 0xE1, 0x3C, 0xFF, 0x2F, 0xE1 };

        u32    *addr = (u32 *)0x00100000;
        u32    *maxAddress = (u32 *)(Process::GetTextSize() + 0x00100000);

        while (addr < maxAddress)
        {
            if (!memcmp(addr, tryOpenFilePat1, sizeof(tryOpenFilePat1)) || !memcmp(addr, tryOpenFilePat2, sizeof(tryOpenFilePat2)))
            {
                FsTryOpenFile = FindNearestSTMFD((u32)addr);
                break;
            }
            addr++;
        }
    }
    static u32      FsTryOpenFileCallback(u32 a1, u16 *fileName, u32 mode);
    static bool     InitFsTryOpenFileHook(void)
    {
        static bool isInitialized = false;

        if (isInitialized)
            return isInitialized;

        auto  createReturncode = [](u32 address, u32 *buf)
        {
            Process::CopyMemory(buf, (void *)address, 8);
            buf[2] = 0xE51FF004;
            buf[3] = address + 8;
        };

        // Hook on OpenFile
        u32     FsTryOpenFileAddress = 0;

        FindFunction(FsTryOpenFileAddress);

        // Check that we found the function
        if (FsTryOpenFileAddress != 0)
        {
            // Create lock
            LightLock_Init(&g_OpenFileLock);

            // Initialize the return code
            createReturncode(FsTryOpenFileAddress, g_returncode);

            // Initialize the hook
            g_FsTryOpenFileHook.flags.useLinkRegisterToReturn = false;
            g_FsTryOpenFileHook.flags.ExecuteOverwrittenInstructionBeforeCallback = false;
            g_FsTryOpenFileHook.Initialize(FsTryOpenFileAddress, (u32)FsTryOpenFileCallback);
            g_FsTryOpenFileAddress = FsTryOpenFileAddress;
            isInitialized = true;
        }
        else
        {
            OSD::Notify("Error: couldn't find OpenFile function");
            // Disable the option
            Preferences::DisplayFilesLoading = false;
        }

        return isInitialized;
    }

    static u32      FsTryOpenFileCallback(u32 a1, u16 *fileName, u32 mode)
    {
        u8      buffer[256] = {0};
        std::string str;
        int     units;

        LightLock_Lock(&g_OpenFileLock);

        if (g_HookMode & OSD)
        {
            // Convert utf16 to utf8
            units = utf16_to_utf8(buffer, fileName, 255);

            if (units > 0)
            {
                str = (char *)buffer;
                OSD::Notify(str);
            }
        }

        if (g_HookMode & FILE)
        {
            // Convert utf16 to utf8 if necessary
            if (str.empty())
            {
                units = utf16_to_utf8(buffer, fileName, 255);
                if (units > 0)
                    str = (char *)buffer;
            }

            // If string isn't empty, write to file
            if (!str.empty())
            {
                g_hookExportFile.WriteLine(str);
            }
        }

        LightLock_Unlock(&g_OpenFileLock);

        return (((FsTryOpenFileType)g_returncode)(a1, fileName, mode));
    }

    static void    _DisplayLoadedFiles(MenuEntryTools *entry)
    {
        // If we must enable the hook
        if (entry->WasJustActivated())
        {
            // Initialize hook
            if (!InitFsTryOpenFileHook())
                entry->Disable(); ///< Hook failed

            // Enable the hook
            Preferences::DisplayFilesLoading = true;
            g_HookMode |= OSD;
            g_FsTryOpenFileHook.Enable();
            return;
        }
        if (!entry->IsActivated())
        {
            // Disable OSD
            Preferences::DisplayFilesLoading = false;
            g_HookMode &= ~OSD;

            // If there's no task to do on the hook, disable it
            if (g_HookMode == 0)
                g_FsTryOpenFileHook.Disable();
        }
    }

    static void    _WriteLoadedFiles(MenuEntryTools *entry)
    {
        // If we must enable the hook
        if (entry->WasJustActivated())
        {
            // Initialize hook
            if (!InitFsTryOpenFileHook())
                entry->Disable(); ///< Hook failed

            // Open the file
            int     mode = File::READ | File::WRITE | File::CREATE | File::APPEND;
            std::string filename = Utils::Format("[%016llX] - LoadedFiles.txt", Process::GetTitleID());

            if (File::Open(g_hookExportFile, filename, mode) != 0)
            {
                OSD::Notify(std::string("Error: couldn't open \"").append(filename).append("\""), Color::Red, Color::Blank);
                entry->Disable(); ///< Disable the entry
                return;
            }

            static bool firstActivation = true;

            if (firstActivation)
            {
                g_hookExportFile.WriteLine("\r\n\r\n### New log ###\r\n");
                firstActivation = false;
            }

            // Enable the hook
            g_HookMode |= FILE;
            g_FsTryOpenFileHook.Enable();

            return;
        }

        if (!entry->IsActivated())
        {
            // Disable File exporting
            g_HookMode &= ~FILE;
            g_hookExportFile.Flush();
            g_hookExportFile.Close();

            // If there's no task to do on the hook, disable it
            if (g_HookMode == 0)
                g_FsTryOpenFileHook.Disable();
        }
    }

    static bool     ConfirmBeforeProceed(const std::string &task)
    {
        std::string msg = Color::Yellow << "Warning\n\n" << ResetColor() << "Do you really want to " + task + " ?";
        MessageBox  msgBox(msg, DialogType::DialogYesNo);

        return (msgBox());
    }

    static void     Shutdown(void)
    {
        if (ConfirmBeforeProceed("shutdown"))
        {
            srvPublishToSubscriber(0x203, 0);
            Sleep(Seconds(10));
        }
    }

    static void     Reboot(void)
    {
        if (ConfirmBeforeProceed("reboot"))
        {
            svcKernelSetState(7);
            Sleep(Seconds(10));
        }
    }

    void    PluginMenuTools::InitMenu(void)
    {
        // Main menu
        _mainMenu.Append(new MenuEntryTools("About", [] { g_mode = ABOUT; }, Icon::DrawAbout));
        _hexEditorEntry = new MenuEntryTools("Hex Editor", [] { g_mode = HEXEDITOR; }, Icon::DrawGrid);
        _mainMenu.Append(_hexEditorEntry);

        _mainMenu.Append(new MenuEntryTools("Gateway RAM Dumper", [] { g_mode = GWRAMDUMP; }, Icon::DrawRAM));
        _mainMenu.Append(new MenuEntryTools("Miscellaneous", nullptr, Icon::DrawMore, new u32(MISCELLANEOUS)));
        _mainMenu.Append(new MenuEntryTools("Settings", nullptr, Icon::DrawSettings, this));
        _mainMenu.Append(new MenuEntryTools("Shutdown the 3DS", Shutdown, Icon::DrawShutdown));
        _mainMenu.Append(new MenuEntryTools("Reboot the 3DS", Reboot, Icon::DrawRestart));

        // Miscellaneous menu
        _miscellaneousMenu.Append(new MenuEntryTools("Display loaded files", _DisplayLoadedFiles, true));
        _miscellaneousMenu.Append(new MenuEntryTools("Write loaded files to file", _WriteLoadedFiles, true));
        _miscellaneousMenu.Append(new MenuEntryTools("Display touch cursor", [] { Preferences::DrawTouchCursor = !Preferences::DrawTouchCursor; }, true, Preferences::DrawTouchCursor));
        _miscellaneousMenu.Append(new MenuEntryTools("Display touch coord", []{ Preferences::DrawTouchCoord = !Preferences::DrawTouchCoord; }, true, false));
        _miscellaneousMenu.Append(new MenuEntryTools("Display top screen's fps", [] {Preferences::ShowTopFps = !Preferences::ShowTopFps; }, true, Preferences::ShowTopFps));
        _miscellaneousMenu.Append(new MenuEntryTools("Display bottom screen's fps", [] {Preferences::ShowBottomFps = !Preferences::ShowBottomFps; }, true, Preferences::ShowBottomFps));

        // Settings menu
        _settingsMenu.Append(new MenuEntryTools("Change menu hotkeys", MenuHotkeyModifier, Icon::DrawGameController));
        _settingsMenu.Append(new MenuEntryTools("Use floating button", [] { Preferences::UseFloatingBtn = !Preferences::UseFloatingBtn; }, true, Preferences::UseFloatingBtn));
        _settingsMenu.Append(new MenuEntryTools("Auto save enabled cheats", [] { Preferences::AutoSaveCheats = !Preferences::AutoSaveCheats; }, true, Preferences::AutoSaveCheats));
        _settingsMenu.Append(new MenuEntryTools("Auto save favorites", [] { Preferences::AutoSaveFavorites = !Preferences::AutoSaveFavorites; }, true, Preferences::AutoSaveFavorites));
        _settingsMenu.Append(new MenuEntryTools("Auto load enabled cheats at starts", [] { Preferences::AutoLoadCheats = !Preferences::AutoLoadCheats; }, true, Preferences::AutoLoadCheats));
        _settingsMenu.Append(new MenuEntryTools("Auto load favorites at starts", [] { Preferences::AutoLoadFavorites = !Preferences::AutoLoadFavorites; }, true, Preferences::AutoSaveFavorites));
        _settingsMenu.Append(new MenuEntryTools("Load enabled cheats now", [] { Preferences::LoadSavedEnabledCheats(); }, nullptr));
        _settingsMenu.Append(new MenuEntryTools("Load favorites now", [] { Preferences::LoadSavedFavorites(); }, nullptr));

        //_settingsMenu.Append(new MenuEntryTools("Inject B when closing the menu", [] { Preferences::InjectBOnMenuClose = !Preferences::InjectBOnMenuClose; }, true));
    }

    bool    PluginMenuTools::operator()(EventList &eventList, Time &delta)
    {
        if (g_mode == HEXEDITOR)
        {
            if (_hexEditor(eventList))
                g_mode = NORMAL;
            return (false);
        }

        if (g_mode == ABOUT)
        {
            if (!_abouttb.IsOpen())
                _abouttb.Open();
            else
                g_mode = NORMAL;
        }

        if (g_mode == GWRAMDUMP)
        {
            _gatewayRamDumper();
            g_mode = NORMAL;
            return (false);
        }

        // Process Event
        for (int i = 0; i < eventList.size(); i++)
            _ProcessEvent(eventList[i]);

        // Update
        _Update(delta);

        // Render Top
        _RenderTop();

        // Render Bottom
        _RenderBottom();

        // Check buttons

        bool exit = _exit || Window::BottomWindow.MustClose();
        _exit = false;
        return (exit);
    }

    void PluginMenuTools::TriggerHexEditor(bool isEnabled) const
    {
        if (!isEnabled)
        {
            _hexEditorEntry->Hide();
        }
        else
            _hexEditorEntry->Show();
    }

    /*
    ** Process Event
    *****************/
    void    PluginMenuTools::_ProcessEvent(Event &event)
    {
        if (_abouttb.IsOpen())
        {
            _abouttb.ProcessEvent(event);
            return;
        }

        MenuItem    *item = nullptr;
        static int mode = 0;

        int ret = _menu.ProcessEvent(event, &item);

        if (ret == EntrySelected && item != nullptr)
        {
            void *arg = ((MenuEntryTools *)item)->GetArg();

            if (arg == this)
            {
                mode = SETTINGS;
                _menu.Open(&_settingsMenu);
            }
            else if (arg != nullptr && *(u32 *)arg == MISCELLANEOUS)
            {
                mode = MISCELLANEOUS;
                _menu.Open(&_miscellaneousMenu);
            }
        }

        if (ret == MenuClose)
        {
            if (mode == SETTINGS)
            {
                mode = 0;

                int selector = 4;

                if (!_hexEditorEntry->IsVisible()) selector--;
                _menu.Open(&_mainMenu, selector);
            }
            else if (mode == MISCELLANEOUS)
            {
                mode = 0;
                _menu.Open(&_mainMenu, 3);
            }
            else
            {
                _exit = true;
            }
        }
    }

    void PluginMenuTools::_RenderTopMenu(void)
    {

    }

    /*
    ** Render Top
    **************/

    void    PluginMenuTools::_RenderTop(void)
    {
        // Enable renderer
        Renderer::SetTarget(TOP);

        if(_abouttb.IsOpen())
        {
            _abouttb.Draw();
            return;
        }

      /*  // Window
        Window::TopWindow.Draw();

        // Title
        int posY = 25;
        int xx = Renderer::DrawSysString("Tools", 40, posY, 330, Color::Blank);
        Renderer::DrawLine(40, posY, xx, Color::Blank);*/

        _menu.Draw();
    }

    /*
    ** Render Bottom
    *****************/
    void    PluginMenuTools::_RenderBottom(void)
    {
        const Color    &black = Color::Black;
        const Color    &blank = Color::Blank;
        const Color    &dimGrey = Color::BlackGrey;

        // Enable renderer
        Renderer::SetTarget(BOTTOM);

        // Window
        Window::BottomWindow.Draw();

        // Draw Framework version
        {
            static const char *version = VersionStr;
            static const u32 xpos = (320 - Renderer::LinuxFontSize(version)) / 2;

            int posY = 205;
            Renderer::DrawString(version, xpos, posY, blank);
        }
    }

    /*
    ** Update
    ************/
    void    PluginMenuTools::_Update(Time delta)
    {
        /*
        ** Buttons
        *************/
        bool        isTouched = Touch::IsDown();
        IntVector   touchPos(Touch::GetPosition());

        Window::BottomWindow.Update(isTouched, touchPos);
    }
}
