#include "CTRPluginFrameworkImpl/Search/SearchBase.hpp"
#include "CTRPluginFramework/System/Process.hpp"
#include "CTRPluginFramework/System/Directory.hpp"
#include "CTRPluginFramework/Menu/MessageBox.hpp"
#include "CTRPluginFrameworkImpl/System/ProcessImpl.hpp"
#include "CTRPluginFramework/System/Controller.hpp"

namespace CTRPluginFramework
{
    u32     AllocatePool(void **pool)
    {
        u32 size = 0x70000;

        do
        {
            size -= 0x10000;
            *pool = new u8[size];
        } while (*pool == nullptr && size > 0x10000);

        if (*pool == nullptr)
            return (0);

        return (size);
    }

    Search::Search(Search *previous) :
    Error({ 0 }),
    ResultsCount(0),
    Progress(0.f),
    Step(previous == nullptr ? 0 : previous->Step + 1),
    _indexRegion(0),
    _startRegion(0),
    _endRegion(0),
    _achievedRegionSize(0),
    _totalRegionSize(0),
    _currentAddress(0),
    _maxResults(0),
    _resultsInPool(0),
    _resultSize(0),
    _flags(0),
    _header({0}),
    _previous(previous),
    _pool(nullptr)
    {
        // Create files's path
        std::string path = "Search/";
        Process::GetTitleID(path);

        path += std::string("-Step") + std::to_string(Step) + ".bin";

        // Open current directory
        Directory dir;
        if (Directory::Open(dir, "Search", true) == 0)
        {
            // Open file
            int res = File::Open(_file, path, File::READ | File::WRITE | File::CREATE | File::TRUNCATE);

            if (res != 0)
            {
                Error.file = true;
                Error.fsError = res;
            }
        }

        // Write Header to reserve space
        WriteHeader();
    }

    void    Search::Cancel(void)
    {
        // Write results in pool to file
        WriteResults();

        // Correct header
        _header.regions[_indexRegion].endAddress = _currentAddress;
        _header.nbRegions = _indexRegion + 1;
        
        // Update Header
        WriteHeader();

        // Set search time
        SearchTime = clock.Restart();

        // Free pool
        delete[] static_cast<u8 *>(_pool);

        // Set index to 0 so it's available to read results
        _indexRegion = 0;

        // Construct index table
        CreateIndexTable();

    }

    bool    Search::ExecuteSearch(void)
    {
        bool isRegionFinished;

        // If it's the first search
        if (IsFirstSearch(_flags))
        {
            if (IsSpecifiedSearch(_flags))
                isRegionFinished = FirstSearchSpecified();
            else
                isRegionFinished = FirstSearchUnknown();
        }

        // If it's the second search
        else if (IsSecondSearch(_flags))
        {
            // If the previous search wasn't an Unknown search
            if (IsSpecifiedSearch(_previous->_flags))
                isRegionFinished = SecondSearchSpecified();
            else
                isRegionFinished = SecondSearchUnknown();
        }

        // Else it's a subsidiary search
        else
        {
            if (IsSpecifiedSearch(_flags))
                isRegionFinished = SubsidiarySearchSpecified();
            else
                isRegionFinished = SubsidiarySearchUnknown();
        }

        // Update progress
        UpdateProgress();

        // If we finished to search the current region, 
        if (isRegionFinished)
        {
            // Write all results in pool to file
            WriteResults();

            // Check if there's another region to search in
            if (CheckNextRegion())
            {
                // Set search time
                SearchTime = clock.Restart();

                // Update Header
                WriteHeader();

                // Free pool
                delete[] static_cast<u8 *>(_pool);

                // Set index to 0 so it's available to read results
                _indexRegion = 0;

                // Construct index table
                CreateIndexTable();

                return (true);
            }
        }

        // Else if pool is full, write to file
        else if (_resultsInPool >= _maxResults - 4)
            WriteResults();

        // Search is not finished
        return (false);
    }

    const Header    &Search::GetHeader(void) const
    {
        return (_header);
    }

    u32     Search::GetTotalResults(const Header &header) const
    {
        u32  totalResults = 0;

        for (int i = 0; i < _header.nbRegions; i++)
        {
            if (_header.regions[i].startAddress != header.regions[i].startAddress)
                continue;

            totalResults += _header.regions[i].nbResults;
        }

        return (totalResults);
    }

    void    Search::ExtractPreviousHits(void *data, u32 index, u32 structSize, u32 &nbItem, bool forced)
    {
        if (!forced)
        {
            RegionOnFile &region = _header.regions[_indexRegion];

            if (index > region.nbResults || !nbItem)
                return;

            u64 offset = region.fileOffset + (index * structSize);

            // Go to the index position in file
            _file.Seek(offset, File::SET);

            if (nbItem + index > region.nbResults)
                nbItem = region.nbResults - index;

            // Read results to data
            _file.Read(data, structSize * nbItem);
        }
        else
        {
            // Get region from result index
            u32     regionIndex = GetRegionIndex(index);

            RegionOnFile &region = _header.regions[regionIndex];

            // Adjust index to region
            index -= _indexTable[regionIndex].startIndex;

            u64 offset = region.fileOffset + (index * structSize);

            // Go to the index position in file
            _file.Seek(offset, File::SET);

            // Check nbItem
            if (regionIndex == _header.nbRegions - 1 && index + nbItem > region.nbResults)
                nbItem = region.nbResults - index;
            else if (nbItem > ResultsCount)
                nbItem = ResultsCount;

            // Read results to data
            _file.Read(data, structSize * nbItem);
        }        
    }

    void    Search::WriteHeader(void)
    {
        u64  offset = _file.Tell();

        // Go to the beginning of the file
        _file.Seek(0, File::SET);
        // Write the header structure
        _file.Write(&_header, sizeof(_header));

        // If saved offset is > 0, return to it
        if(offset > 0)
            _file.Seek(offset, File::SET);
    }

    void    Search::WriteResults(void)
    {
        if (!_resultsInPool)
            return;

        u32 size = _resultSize * _resultsInPool;

        // Write results to file
        _file.Write(_pool, size);  
        
        // Update header
        _header.regions[_indexRegion].nbResults += _resultsInPool;

        // Reset counter
        _resultsInPool = 0;
    }

    void    Search::UpdateProgress(void)
    {
        u32  excedent = 0;

        if (IsFirstSearch(_flags))
        {
             excedent = _currentAddress - _startRegion;         
        }
        else
        {
            // In subsidiary search, _currentAddress act as an index in previous results
            excedent = _currentAddress;
        }

        Progress = 100.0f * (float)(_achievedRegionSize + excedent) / (float)_totalRegionSize;        
    }

    bool    Search::CheckNextRegion(void)
    {
        // First Search
        if (IsFirstSearch(_flags))
        {
            // If we've done the last region, return true
            if (_indexRegion == _header.nbRegions - 1)
                return (true);

            // Increment index
            _indexRegion++;

            // Increment achieved size for progress calculation
            _achievedRegionSize += _endRegion - _startRegion;
            
            // Set Region's range
            _startRegion = _header.regions[_indexRegion].startAddress;
            _endRegion = _header.regions[_indexRegion].endAddress - 1;

            // Set file's offset in header
            _header.regions[_indexRegion].fileOffset = _file.Tell();

            // Flush memory
            svcFlushProcessDataCache(ProcessImpl::_processHandle, (void *)_startRegion, _endRegion - _startRegion);
            return (false);
        }

        // Subsidiary search
        {
            // If we've done the last region, return true
            if (_indexRegion >= _header.nbRegions)
                return (true);                

            // Increment index
            _indexRegion++;

            // Set Region's range
            _startRegion = _header.regions[_indexRegion].startAddress;

            // Increment achieved size for progress calculation
            _achievedRegionSize += _previous->_header.regions[_previous->_indexRegion].nbResults;

            // Reset current index in region's results
            _currentAddress = 0;

            // Set file's offset in header
            _header.regions[_indexRegion].fileOffset = _file.Tell();

            // Loop through previous's regions to find the one corresponding
            u32     &previousIndex = _previous->_indexRegion;
            Header  &previousHeader = _previous->_header;

            while (previousHeader.regions[previousIndex].startAddress != _startRegion
                && previousIndex < previousHeader.nbRegions)
            {
                previousIndex++;
            }

            // If we're after the last region of the previous header
            // An error occured so exit
            if (previousIndex >= previousHeader.nbRegions)
                return (true);

            // If region have no results, skip it
            if (!previousHeader.regions[previousIndex].nbResults)
                return (CheckNextRegion());

            // Set end region
            _endRegion = previousHeader.regions[previousIndex].nbResults;

            // Set previous file's offset
            u64 offset = previousHeader.regions[previousIndex].fileOffset;

            _previous->_file.Seek(offset, File::SET);

            // Flush memory
            svcFlushProcessDataCache(ProcessImpl::_processHandle, (void *)_startRegion, _header.regions[_indexRegion].endAddress - _startRegion);

            return (false);
        }
    }

    void    Search::CreateIndexTable(void)
    {
        u32 resultCount = 0;

        for (int i = 0; i < _header.nbRegions; i++)
        {
            RegionIndex rindex;

            rindex.startIndex = resultCount;
            resultCount += _header.regions[i].nbResults;
            rindex.endIndex = resultCount;

            _indexTable.push_back(rindex);
        }
    }

    u32     Search::GetRegionIndex(u32 index)
    {
        u32 i = 0;
        for (RegionIndex &rindex : _indexTable)
        {
            if (index >= rindex.startIndex && index <= rindex.endIndex)
                return (i);
            i++;
        }

        return (i); ///< Not safe but shouldn't happen
    }
}
