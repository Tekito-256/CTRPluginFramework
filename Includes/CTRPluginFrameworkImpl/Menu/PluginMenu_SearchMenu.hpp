#ifndef CTRPLUGINFRAMEWORKIMPL_SearchMenu_HPP
#define CTRPLUGINFRAMEWORKIMPL_SearchMenu_HPP


#include "CTRPluginFrameworkImpl/Graphics.hpp"
#include "CTRPluginFrameworkImpl/System.hpp"
#include "CTRPluginFrameworkImpl/Menu/PluginMenuSearchStructs.hpp"
#include "CTRPluginFramework/System/File.hpp"

#include <string>
#include <vector>

namespace CTRPluginFramework
{
    class SearchMenu
    {
        using EventList = std::vector<Event>;
    public:

        SearchMenu(SearchBase* &curSearch);
        ~SearchMenu(){};

        bool    ProcessEvent(EventList &eventList, Time &delta);
        void    Draw(void);
        void    Update(void);

    private:

        SearchBase*                 &_currentSearch;
        std::vector<std::string>    _resultsAddress;
        std::vector<std::string>    _resultsNewValue;
        std::vector<std::string>    _resultsOldValue;
        std::vector<std::string>    _options;

        int                         _selector;
        int                         _submenuSelector;
        u32                         _index;
        bool                        _isSubmenuOpen;
        bool                        _action;
        bool                        _alreadyExported;
        Clock                       _buttonFade;
        File                        _export;

        void        _DrawSubMenu(void);
        void        _Save(void);
        void        _Edit(void);
        void        _JumpInEditor(void);
        void        _Export(void);
        void        _ExportAll(void);

    };
}

#endif