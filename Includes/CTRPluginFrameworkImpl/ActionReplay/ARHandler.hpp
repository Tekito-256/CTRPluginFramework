#ifndef CTRPLUGINFRAMEWORKIMPL_ACTIONREPLAY_ARHANDLER_HPP
#define CTRPLUGINFRAMEWORKIMPL_ACTIONREPLAY_ARHANDLER_HPP

#include "types.h"
#include "CTRPluginFrameworkImpl/ActionReplay/ARCode.hpp"

namespace CTRPluginFramework
{
    class ARHandler
    {
    public:
        struct Register
        {
        public:
            Register()
            {
                Clear();
            }

            void Clear(void)
            {
                isVFP = false;
                value = 0;
            }

            bool isVFP;

            union
            {
                u32 value;
                float vfp;
            };
        };

        static u32  Offset[2];
        static Register  Data[2];
        static u32  Storage[2];
        static u32  ActiveOffset;
        static u32  ActiveData;
        static bool ExitCodeImmediately;

        static void Execute(const ARCodeVector &arcodes, u32(&storage)[2]);

    private:
        static void _Execute(const ARCodeVector &codes);
    };
}

#endif
