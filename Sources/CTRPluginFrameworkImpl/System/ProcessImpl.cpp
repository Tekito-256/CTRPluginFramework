#include "3DS.h"
#include "CTRPluginFramework/System.hpp"
#include "CTRPluginFrameworkImpl/System.hpp"
#include "CTRPluginFrameworkImpl/arm11kCommands.h"
#include "CTRPluginFrameworkImpl/Preferences.hpp"

#include <cstdio>
#include <cstring>
#include "ctrulib/gpu/gpu.h"
#include "CTRPluginFrameworkImpl/Graphics/OSDImpl.hpp"

extern 		Handle gspThreadEventHandle;

namespace CTRPluginFramework
{
	Handle      ProcessImpl::ProcessHandle = 0;
	u32         ProcessImpl::IsPaused = 0;
    u32         ProcessImpl::ProcessId = 0;
    u64         ProcessImpl::TitleId = 0;

    KThread *   ProcessImpl::MainThread;
    KProcess *  ProcessImpl::KProcessPtr;
    KCodeSet    ProcessImpl::CodeSet;

	void    ProcessImpl::Initialize(void)
	{
		char    kproc[0x100] = {0};
		bool 	isNew3DS = System::IsNew3DS();

		// Get current KProcess
		KProcessPtr = reinterpret_cast<KProcess *>(arm11kGetCurrentKProcess());

		// Copy KProcess data
		arm11kMemcpy(kproc, KProcessPtr, 0x100);

		if (isNew3DS)
		{
			// Copy KCodeSet
			arm11kMemcpy(&CodeSet, (void *)*(u32 *)(kproc + 0xB8), sizeof(KCodeSet));

			// Copy process id
			ProcessId = *(u32 *)(kproc + 0xBC);

            // Get main thread
            MainThread = (KThread *)*(u32 *)(kproc + 0xC8);

            // Patch KProcess to allow creating threads on Core2
            arm11kAllowCore2();
		}
		else
		{
			// Copy KCodeSet
			arm11kMemcpy(&CodeSet, (void *)*(u32 *)(kproc + 0xB0), sizeof(KCodeSet));

			// Copy process id
			ProcessId = *(u32 *)(kproc + 0xB4);

            // Get main thread
            MainThread = (KThread *)*(u32 *)(kproc + 0xC0);
		}

		// Copy title id
		TitleId = CodeSet.titleId;
		// Create handle for this process
		svcOpenProcess(&ProcessHandle, ProcessId);
	}

    extern "C" Handle gspThreadEventHandle;;
    extern "C" Handle gspEvent;
    extern "C" bool   IsPaused(void)
    {
        return (ProcessImpl::IsPaused > 0);
    }

	void 	ProcessImpl::Pause(bool useFading)
	{
        // Increase pause counter
        ++IsPaused;

        // If game is already paused, nothing to do
        if (IsPaused > 1)
            return;

        // Wake up gsp event thread
        svcSignalEvent(gspEvent);

        // Wait for the frame to be paused
        OSDImpl::WaitFramePaused();

        // Wait for the vblank
        gspWaitForVBlank();

        // Acquire screens
        ScreenImpl::Top->Acquire();
        ScreenImpl::Bottom->Acquire();
        OSDImpl::UpdateScreens();

        if (!useFading)
            return;

     /*   float fade = 0.03f;
        Clock t = Clock();
        Time limit = Seconds(1) / 10.f;
        Time delta;
        float pitch = 0.0006f;

        while (fade <= 0.3f)
        {
        	delta = t.Restart();
        	fade += pitch * delta.AsMilliseconds();

        	ScreenImpl::Top->Fade(fade);
        	ScreenImpl::Bottom->Fade(fade);

        	ScreenImpl::Top->SwapBuffer(true, true);
        	ScreenImpl::Bottom->SwapBuffer(true, true);
        	gspWaitForVBlank();
        	if (System::IsNew3DS())
        		while (t.GetElapsedTime() < limit);
        } */

        ScreenImpl::ApplyFading();
	}

	void 	ProcessImpl::Play(bool useFading)
	{
        // If game isn't paused, abort
        if (!IsPaused)
            return;

		/*if (useFading)
		{
            Time limit = Seconds(1) / 10.f;
            Time delta;
            float pitch = 0.10f;
            Clock t = Clock();
	        float fade = -0.1f;
            while (fade >= -0.9f)
            {
                delta = t.Restart();
                ScreenImpl::Top->Fade(fade);
                ScreenImpl::Bottom->Fade(fade);
                fade -= 0.001f * delta.AsMilliseconds();
                //Sleep(Milliseconds(10));
                ScreenImpl::Top->SwapBuffer(true, true);
                ScreenImpl::Bottom->SwapBuffer(true, true);
                gspWaitForVBlank();
                if (System::IsNew3DS())
                    while (t.GetElapsedTime() < limit); //<- On New3DS frequencies, the alpha would be too dense
            }
		} */

        // Decrease pause counter
        if (IsPaused)
            --IsPaused;

        // Resume frame
        if (!IsPaused)
            OSDImpl::ResumeFrame();
	}

    bool     ProcessImpl::PatchProcess(u32 addr, u8 *patch, u32 length, u8 *original)
    {
 		if (original != nullptr)
 		{
 			if (!Process::CopyMemory((void *)original, (void *)addr, length))
 				goto error;
 		}

 		if (!Process::CopyMemory((void *)addr, (void *)patch, length))
 			goto error;

        return (true);
    error:
        return (false);
    }

    void    ProcessImpl::GetHandleTable(KProcessHandleTable& table, std::vector<HandleDescriptor>& handleDescriptors)
    {
        bool 	isNew3DS = System::IsNew3DS();

        // Copy KProcessHandleTable
        arm11kMemcpy(&table, (void *)((u32)KProcessPtr + (isNew3DS ? 0xDC : 0xD4)), sizeof(KProcessHandleTable));

        u32 count = table.handlesCount;

        handleDescriptors.resize(count);
        arm11kMemcpy(handleDescriptors.data(), table.handleTable, count * sizeof(HandleDescriptor));
    }
}
