#include "CTRPluginFrameworkImpl/Graphics/Textbox.hpp"
#include "CTRPluginFramework/System/Clock.hpp"
#include "types.h"
#include "ctrulib/util/utf.h"
#include "ctrulib/font.h"
#include "CTRPluginFrameworkImpl/Preferences.hpp"

#include <cstdio>
#include <math.h>

namespace CTRPluginFramework
{
    /*
    ** Constructor
    ***************/
    TextBox::TextBox(std::string title, std::string &text, IntRect box) :
    _title(title), _text(text), _box(box)
    {
        _currentLine = 0;
        _isOpen = false;
        _maxLines = ((box.size.y - 10) / 16) - 1;        
        _border = IntRect(box.leftTop.x + 2, box.leftTop.y + 2, box.size.x - 4, box.size.y - 4);
        _GetTextInfos();
        if (_newline.size() < _maxLines)
        {
            _maxLines = _newline.size();
            _displayScrollbar = false;
        }
        else
        {
            int height = _box.size.y;

            height -= 10;

            int lines = _newline.size() + 1;
            int lsize = 16 * lines;

            float padding = (float)height / (float)lsize;
            int cursorSize =  padding * height;
            _scrollbarSize = height;

            if (cursorSize < 5)
                cursorSize = 5;

            _scrollCursorSize = cursorSize;
            _scrollPadding = padding * 16.f;
            _scrollPosition = 0.f;
            _displayScrollbar = true;
        }
        Color blank(255, 255, 255);
        titleColor = blank;
        textColor = blank;
        borderColor = blank;
    }

    /*
    ** Open
    ************/
    void    TextBox::Open(void)
    {
        _isOpen = true;
    }

    /*
    ** Close
    ***********/
    void    TextBox::Close(void)
    {
        _isOpen = false;
    }

    /*
    ** IsOpen
    ***********/
    bool    TextBox::IsOpen(void)
    {
        return (_isOpen);
    }

    /*
    ** Process Event
    *****************/
    bool    TextBox::ProcessEvent(Event &event)
    {
        if (!_isOpen)
            return (false);
        
        if (event.type == Event::KeyDown)
        {
            switch (event.key.code)
            {
                case Key::DPadUp:
                case Key::CPadUp:
                {
                    if (_inputClock.HasTimePassed(Milliseconds(100)))
                    {
                        if (_currentLine > 0)
                        {
                            _currentLine--;
                            _scrollPosition -= _scrollPadding;; //_scrollPadding;
                        }
                        //else
                         //   _currentLine = _newline.size() - _maxLines;
                        _inputClock.Restart();
                    }
                    break;
                }
                case Key::DPadDown:
                case Key::CPadDown:
                {
                    if (_inputClock.HasTimePassed(Milliseconds(100)))
                    {   
                        if (_currentLine < _newline.size() - _maxLines)
                        {
                            _currentLine++;
                            _scrollPosition += _scrollPadding;
                        }
                        //else
                        //    _currentLine = 0;
                        _inputClock.Restart();
                    }
                    break;
                }
                case Key::DPadLeft:
                {
                    _scrollPosition = 0.f;
                    _currentLine = 0;
                    break;
                }
                case Key::DPadRight:
                {
                    _scrollPosition = (_newline.size() - _maxLines) * _scrollPadding;
                    _currentLine = std::max((int)(_newline.size() - _maxLines), 0);
                    break;
                }
                case Key::CStickUp:
                {
                    if (_inputClock.HasTimePassed(Milliseconds(100)))
                    {
                        if (_currentLine > 1)
                        {
                            _currentLine -= 2;
                            _scrollPosition -= _scrollPadding * 2;
                        }
                        //else
                         //   _currentLine = _newline.size() - _maxLines;
                        _inputClock.Restart();
                    }
                    break;
                }
                case Key::CStickDown:
                {
                    if (_inputClock.HasTimePassed(Milliseconds(100)))
                    {   
                        if (_currentLine < _newline.size() - _maxLines)
                        {
                            _currentLine += 2;
                            _scrollPosition += _scrollPadding * 2;
                        }
                        //else
                        //    _currentLine = 0;
                        _inputClock.Restart();
                    }
                    break;
                }
                case Key::B:
                {
                    Close();
                    return (false);
                    break;
                }
            }
        }
        return (true);
    }

    /*
    ** Draw
    ***********/
    void    TextBox::Draw(void)
    {
        if (_currentLine >= _newline.size())
            _currentLine = 0;

        int  max = std::min((u32)(_currentLine + _maxLines), (u32)(_newline.size()));

        Color black = Color();
        Color blank = Color(255, 255, 255);
        Color grey = Color(15, 15, 15);

        // Draw Background
        if (Preferences::topBackgroundImage->IsLoaded() 
            && (Preferences::topBackgroundImage->GetDimensions() <= _box.size))
            Preferences::topBackgroundImage->Draw(_box, -0.3f);
        else
        {
            Renderer::DrawRect2(_box, black, grey);
            Renderer::DrawRect(_border, borderColor, false);            
        }


        int posX = _box.leftTop.x + 5;
        int posY = _box.leftTop.y + 5;
        int xLimit = posX + _box.size.x - 5;

        // Draw Title
        int width;
        width = Renderer::DrawSysString((const char *)_title.c_str(), posX, posY, xLimit, titleColor);
        Renderer::DrawLine(posX, posY, width - posX + 30, blank);
        posY += 7;

        // Draw Text
        for (int i = _currentLine; i < max; i++)
        {
            Renderer::DrawSysString(_newline[i], posX, posY, xLimit, textColor, 0, _newline[i + 1]);
        }

        if (!_displayScrollbar)
            return;

        // Draw scroll bar
        static Color dimGrey(105, 105, 105);
        static Color silver(192, 192, 192);

        // Background
        posX = _box.leftTop.x + _box.size.x - 8;
        posY = _box.leftTop.y + 5;

        Renderer::DrawLine(posX, posY + 1, 1, silver, _scrollbarSize - 2);
        Renderer::DrawLine(posX + 1, posY, 1, silver, _scrollbarSize);
        Renderer::DrawLine(posX + 2, posY + 1, 1, silver, _scrollbarSize - 2);

        posY += (int)(_scrollPosition);// * _scrollbarSize);

        Renderer::DrawLine(posX, posY + 1, 1, dimGrey, _scrollCursorSize - 2);
        Renderer::DrawLine(posX + 1, posY, 1, dimGrey, _scrollCursorSize);
        Renderer::DrawLine(posX + 2, posY + 1, 1, dimGrey, _scrollCursorSize - 2);        

    }

    /*
    ** Text infos
    ***************/

    char   *TextBox::_GetWordWidth(char *str, float &width, float &extra, int &count)
    {
        width = 0;
        count = 0;
        extra = 0;  

        u32             code;
        ssize_t         units = decode_utf8(&code, (u8 *)str);
        if (units == -1 || code == 0)
            return (nullptr);

        float           spaceWidth;
        u32             spaceCode;
        ssize_t         spaceUnits;
        spaceUnits = decode_utf8(&spaceCode, (u8 *)" ");
        int             glyphIndex;
        fontGlyphPos_s  data;
        charWidthInfo_s *cwi;

        glyphIndex = fontGlyphIndexFromCodePoint(code);         
        Renderer::FontCalcGlyphPos(&data, &cwi, glyphIndex, 0.5f, 0.5f);
        spaceWidth = data.xAdvance + 1;
        
        while (*str != '\n' && *str != ' ')
        {
            units = decode_utf8(&code, (u8 *)str);

            if (units == -1 || code == 0)
                return (nullptr);

            str += units;
            count += units;
            if (code == spaceCode)
            {
                extra += spaceWidth;
                break;
            }

            glyphIndex = fontGlyphIndexFromCodePoint(code);         
            Renderer::FontCalcGlyphPos(&data, &cwi, glyphIndex, 0.5f, 0.5f);
            width += ceilf(data.xAdvance) + 1;
        }
        if (*str == ' ')
        {
            str += spaceUnits;
            extra = spaceWidth;
            count += spaceUnits;
        }
        return (str);
    }

    /*
    ** Get text infos
    ******************/
    void     TextBox::_GetTextInfos(void)
    {
        float           w = 0.0f;
        char            *str = (char *)_text.c_str();

        float maxWidth = _border.size.x - 5.f;

        _newline.push_back(str);
        int advance = 0;
        while (*str == '\n')
        {
            _newline.push_back(str);
            str++;
            advance++;
        }
        
        while (1)
        {
            if (*str == '\n')
            {
                str++;
            }
            char *s = str;            
            float line = 0.f;
            int ncount = 0;


            while (line < maxWidth && s != nullptr)
            {
                str = s;
                float ww;
                float extra;
                int count = 0;

                s = _GetWordWidth(s, ww, extra, count);                

                line += ww;
                // if we are at the end of the text
                if (s == nullptr)
                {
                    // if line is shorter than the border
                    if (line < maxWidth)
                    {
                        goto exit;
                    }
                    else
                    {
                        _newline.push_back(str);
                        goto exit;
                    }
                }

                // If we have a new line symbol
                if (*s == '\n' && line < maxWidth)
                {
                    str = s;
                    break;
                }

                // If the espace char is out of border, skip it and go to a new line
                if (line < maxWidth && line + extra >= maxWidth)
                {                    
                    str = s;
                    break;                                
                }
                line += extra;
            }
            _newline.push_back(str);

        }
    exit:
        _newline.push_back(nullptr);
    }
}