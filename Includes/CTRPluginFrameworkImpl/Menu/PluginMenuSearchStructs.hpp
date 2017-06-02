#ifndef CTRPLUGINFRAMEWORKIMPL_SEARCHSTRUCT_HPP
#define CTRPLUGINFRAMEWORKIMPL_SEARCHSTRUCT_HPP

#include "types.h"

#include "CTRPluginFramework/System/File.hpp"
#include "CTRPluginFramework/System/Clock.hpp"
#include <vector>
#include <iterator>

namespace CTRPluginFramework
{
    struct Region
    {
        u32 startAddress;
        u32 endAddress;
    };

    struct SRegion
    {
        u32 startAddress;
        u32 endAddress;
        u64 fileOffset;
    };
   

    enum SearchFlags : u8
    {
        Subsidiary = 1,
        FirstExact = 1 << 1,
        FirstUnknown = 1 << 2
    };

    enum class SearchSize : u8
    {
        Bits8 = 0,
        Bits16,
        Bits32,
        Bits64,
        FloatingPoint,
        Double
    };

    enum class  SearchType : u8
    {
        ExactValue = 0,
        Unknown = 1
    };

    enum class CompareType : u8
    {
        Equal = 0,
        NotEqual,
        GreaterThan,
        GreaterOrEqual, 
        LesserThan,
        LesserOrEqual,
        DifferentBy,
        DifferentByLess,
        DifferentByMore
    };

    template <typename T>
    struct SearchResultFirst
    {
        SearchResultFirst() : address(0), value(0) {}
        SearchResultFirst(u32 addr, T v) : address(addr), value(v) {}

        u32     address;
        T       value;
    };

    template <typename T>
    struct SearchResult
    {
        SearchResult() : address(0), value(0) {}
        SearchResult(u32 addr, T v) : address(addr), value(v) {}

        u32     address;
        T       value;
        T       oldValue;
    };

    template <typename T>
    struct SearchResultUnknown
    {
        SearchResultUnknown() : value(0) {}
        SearchResultUnknown(u32 addr, T v) :value(v) {}

        T       value;
    };

    class   SearchBase
    {
        using stringvector = std::vector<std::string>;

    public:

        SearchBase(u32 start, u32 end, u32 alignment, SearchBase *prev);
        virtual ~SearchBase(){}

        virtual void    Cancel(void) = 0;
        virtual bool    DoSearch(void) = 0;
        virtual bool    ResultsToFile(void) = 0;
        virtual bool    FetchResults(stringvector &address, stringvector &newval, stringvector &oldvalue, u32 index, int count) = 0;
        //virtual u32     GetHeaderSize(void) = 0;

        SearchType      Type; 
        SearchSize      Size;
        CompareType     Compare;

        u32             ResultCount; //<- results counts
        float           Progress; //<- current progression of the search in %
        Time            SearchTime;
        u32             Step; //<- current step

        SearchBase *_previousSearch;
    protected: 
        friend class SearchMenu;
        
        u32                     _startRange; //<- Start address
        u32                     _endRange; //<- End address
        u32                     _currentPosition; //<- Current position in the search range
        u8                      _alignment; //<- search alignment
        bool                    _fullRamSearch;
        int                     _currentRegion;
        u32                     _totalSize;
        u32                     _achievedSize;
        std::vector<SRegion>    &_regionsList;
        Clock                   _clock;
        
        // Hit list building var
        u32                     _lastFetchedIndex;
        u32                     _lastStartIndexInFile;
        u32                     _lastEndIndexInFile;

        File                    _file; //<- File associated for read / Write

        

    };
    
    //template <typename T>
    

    template <typename T>
    class Search : public SearchBase
    {
        using ResultIter = typename std::vector<SearchResult<T>>::iterator;
        using ResultExactIter = typename std::vector<SearchResultFirst<T>>::iterator;
        using ResultUnknownIter = typename std::vector<SearchResultUnknown<T>>::iterator;
        using UnknResultIter = typename std::vector<SearchResultUnknown<T>>::iterator;
        using stringvector = std::vector<std::string>;
        
        using CmpFunc = bool (Search<T>::*)(T, T);

    public:

        Search(T value, u32 start, u32 end, u32 alignment, SearchBase *previous);
        Search(T value, std::vector<Region> &list, u32 alignment, SearchBase *previous);
        ~Search(){}
        /*
        ** Cancel
        */
        void    Cancel(void) override;
        /*
        ** DoSearch
        ** Override SearchBase's
        ** Return true if search is done
        ****************************/
        bool    DoSearch(void) override;

        /*
        ** ResultsToFile
        ** Override SearchBase's
        ** Return true if suceeded
        ****************************/
        bool    ResultsToFile(void) override;

        bool    FetchResults(stringvector &address, stringvector &newval, stringvector &oldvalue, u32 index, int count) override;

    private:

        T                               _checkValue; //<- Value to compare with
        u32                             _maxResult;
        void                            *_resultsArray;
        void                            *_resultsEnd;
        void                            *_resultsP;
        SearchFlags                     _flags;
        CmpFunc                         _compare;

        void _FirstUnknownSearch(u32& start, const u32 end);
        /*
        ** Methods
        ***********/
        bool        _SecondExactSearch(void);
        bool        _SecondUnknownSearch(void);
        bool        _SubsidiarySearch(void);
        bool        _SearchInRange(void);
        void        _FirstExactSearch(u32 &start, u32 end);
        void        _UpdateCompare(void);
        // Return true if the value matches the search settings
        //bool        _Compare(T old, T newer);

        bool        _CompareE(T old, T newer);
        bool        _CompareNE(T old, T newer);
        bool        _CompareGT(T old, T newer);
        bool        _CompareGE(T old, T newer);
        bool        _CompareLT(T old, T newer);
        bool        _CompareLE(T old, T newer);
        bool        _CompareDB(T old, T newer);
        bool        _CompareDBL(T old, T newer);
        bool        _CompareDBM(T old, T newer);

        bool        _CompareUnknownE(T old, T newer);
        bool        _CompareUnknownNE(T old, T newer);
        bool        _CompareUnknownGT(T old, T newer);
        bool        _CompareUnknownGE(T old, T newer);
        bool        _CompareUnknownLT(T old, T newer);
        bool        _CompareUnknownLE(T old, T newer);
        
        bool        _WriteHeaderToFile(void);
        static u32  _GetHeaderSize(void);
        u32         _GetResultStructSize(void) const;        
        bool        _ReadResults(std::vector<SearchResult<T>> &out, u32 &index, u32 count);
        bool        _ReadResults(std::vector<SearchResultUnknown<T>> &out, u32 &index, u32 count);
        bool        _ReadResults(std::vector<SearchResultFirst<T>>& out, u32& index, u32 count);

        template <typename U>
        static int ReadFromFile(File &file, U &t)
        {
            return (file.Read(reinterpret_cast<char *>(&t), sizeof(U)));
        }

        template <typename U>
        static int WriteToFile(File &file, const U &t)
        {
            return (file.Write(reinterpret_cast<const char *>(&t), sizeof(U)));
        }
    };
}

#endif