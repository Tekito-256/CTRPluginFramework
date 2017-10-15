#include "types.h"
#include "3DS.h"
#include "CTRPluginFramework/System/System.hpp"
#include "CTRPluginFrameworkImpl/System/SystemImpl.hpp"

namespace CTRPluginFramework
{
    bool    System::IsNew3DS(void)
    {
        return (SystemImpl::_isNew3DS);
    }

    Language    System::GetSystemLanguage(void)
    {
        return (static_cast<Language>(SystemImpl::_language));
    }

    bool    System::IsConnectedToInternet(void)
    {
        u32 out = 0;
        u8  *wifiLevel = (u8 *)0x1FF81066;

        if (R_FAILED(ACU_GetWifiStatus(&out)))
            out = 0;

        return (out != 0 && *wifiLevel != 0);
    }

    /// Retrieves the major version from a packed system version.
#define GET_VERSION_MAJOR(version)    ((version) >>24)

    /// Retrieves the minor version from a packed system version.
#define GET_VERSION_MINOR(version)    (((version)>>16)&0xFF)

    /// Retrieves the revision version from a packed system version.
#define GET_VERSION_REVISION(version) (((version)>> 8)&0xFF)

    bool    System::IsCfwLuma3DS(const u8 major, const u8 minor, const u8 revision)
    {
        if (SystemImpl::_CFWVersion == 0 || !major)
            return (false);

        u8  _major = GET_VERSION_MAJOR(SystemImpl::_CFWVersion);
        u8  _minor = GET_VERSION_MINOR(SystemImpl::_CFWVersion);
        u8  _revision = GET_VERSION_REVISION(SystemImpl::_CFWVersion);

        if (_major < major) return (false);
        if (_minor < minor) return (false);
        return (_revision >= revision);
    }
}
