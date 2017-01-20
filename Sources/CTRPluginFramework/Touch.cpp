#include "CTRPluginFramework.hpp"

namespace CTRPluginFramework
{
    bool        Touch::IsDown(void)
    {
        return (Controller::IsKeyDown(Key::Touchpad));
    }

    UIntVector  Touch::GetPosition(void)
    {
        touchPosition   tp;

        hidTouchRead(&tp);
        return (UIntVector(tp.px, tp.py));
    }
}