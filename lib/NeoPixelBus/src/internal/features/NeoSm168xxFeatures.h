/*-------------------------------------------------------------------------
NeoSm168xxFeatures provides feature classes to describe color order and
color depth for NeoPixelBus template class specific to the SM168xx chips/leds

Written by Michael C. Miller.

I invest time and resources providing this open source code,
please support me by dontating (see https://github.com/Makuna/NeoPixelBus)

-------------------------------------------------------------------------
This file is part of the Makuna/NeoPixelBus library.

NeoPixelBus is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

NeoPixelBus is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with NeoPixel.  If not, see
<http://www.gnu.org/licenses/>.
-------------------------------------------------------------------------*/
#pragma once
/*
3 channel RGB
SM16803P  1.8~60mA << need spec sheet to get accurate implementation
SM16813PB 1.8~19mA 
SM16823E 60~350mA
4 channel RGBW
SM16804PB 1.5~60mA << need spec sheet to get accurate implementation
SM16804EB 1.8~19mA   
SM16824E 60~350mA
*/

#include "../NeoUtil.h"

class NeoSm168x3SettingsBase : public NeoRgbCurrentSettings
{
public:
    NeoSm168x3SettingsBase(uint8_t redGain, 
            uint8_t greenGain, 
            uint8_t blueGain,
            uint16_t redCurrent, 
            uint16_t greenCurrent,
            uint16_t blueCurrent) :
        NeoRgbCurrentSettings(redCurrent, greenCurrent, blueCurrent),
        RedGain(redGain & 0x0f),
        GreenGain(greenGain & 0x0f),
        BlueGain(blueGain & 0x0f) {}

    // ------------------------------------------------------------------------
    // operator [] - readonly
    // access elements in order by index rather than member name
    // ------------------------------------------------------------------------
    uint8_t operator[](size_t idx) const
    {
        switch (idx)
        {
        case 0:
            return RedGain;
        case 1:
            return GreenGain;
        default:
            return BlueGain;
        }
    }

    const uint8_t RedGain : 4;
    const uint8_t GreenGain : 4;
    const uint8_t BlueGain : 4;
};

template <uint8_t V_IC_1, uint8_t V_IC_2, uint8_t V_IC_3>
class NeoSm16803pbSettings : public NeoSm168x3SettingsBase
{
public:
    NeoSm16803pbSettings(uint8_t redGain, uint8_t greenGain, uint8_t blueGain) :
        NeoSm168x3SettingsBase(redGain, 
            greenGain, 
            blueGain,
            CurrentLookup[redGain],
            CurrentLookup[greenGain],
            CurrentLookup[blueGain])
    {
    }

    void Encode(uint8_t* encoded) const
    {
        // 0RGB 4 bits each
        *encoded++ = operator[](V_IC_1);
        *encoded = operator[](V_IC_2) << 4 | operator[](V_IC_3);
    }

protected:
    static constexpr uint8_t CurrentLookup[16] = {
            18, 30, 41, 53, 64, 76, 87, 99, 
            110, 133, 145, 156, 168, 179, 190};
};

template <uint8_t V_IC_1, uint8_t V_IC_2, uint8_t V_IC_3>
class NeoSm16823eSettings : public NeoSm168x3SettingsBase
{
public:
    NeoSm16823eSettings(uint8_t redGain, uint8_t greenGain, uint8_t blueGain, uint16_t resisterOhms) :
        NeoSm168x3SettingsBase(redGain,
            greenGain,
            blueGain,
            calcCurrent(resisterOhms, redGain),
            calcCurrent(resisterOhms, greenGain),
            calcCurrent(resisterOhms, blueGain)),
        extROhms(resisterOhms)
    {
    }

    void Encode(uint8_t* encoded) const
    {
        // RGB0 4 bits each
        *encoded++ = operator[](V_IC_1) << 4 | operator[](V_IC_2);
        *encoded = operator[](V_IC_3) << 4;
    }

protected:
    const uint16_t extROhms;

    static uint16_t calcCurrent(const uint16_t ohms, const uint8_t gain)
    {
        uint16_t mA = (967 * (240 + (gain * 32)) / ohms); // from spec sheet, gain 0-15 instead
        return mA * 10; // return tenths of mA
    }
};

// RGBW versions

class NeoSm168x4SettingsBase : public NeoRgbwCurrentSettings
{
public:
    NeoSm168x4SettingsBase(uint8_t redGain, 
            uint8_t greenGain, 
            uint8_t blueGain, 
            uint8_t whiteGain,
            uint16_t redCurrent,
            uint16_t greenCurrent,
            uint16_t blueCurrent,
            uint16_t whiteCurrent) :
        NeoRgbwCurrentSettings(redCurrent, greenCurrent, blueCurrent, whiteCurrent),
        RedGain(redGain & 0x0f),
        GreenGain(greenGain & 0x0f),
        BlueGain(blueGain & 0x0f),
        WhiteGain(whiteGain & 0x0f) {}

    // ------------------------------------------------------------------------
    // operator [] - readonly
    // access elements in order by index rather than member name
    // ------------------------------------------------------------------------
    uint8_t operator[](size_t idx) const
    {
        switch (idx)
        {
        case 0:
            return RedGain;
        case 1:
            return GreenGain;
        case 2:
            return BlueGain;
        default:
            return WhiteGain;
        }
    }

    const uint8_t RedGain : 4;
    const uint8_t GreenGain : 4;
    const uint8_t BlueGain : 4;
    const uint8_t WhiteGain : 4;
};

template <uint8_t V_IC_1, uint8_t V_IC_2, uint8_t V_IC_3, uint8_t V_IC_4>
class NeoSm16804ebSettings : public NeoSm168x4SettingsBase
{
public:
    NeoSm16804ebSettings(uint8_t redGain, uint8_t greenGain, uint8_t blueGain, uint8_t whiteGain) :
        NeoSm168x4SettingsBase(redGain, 
            greenGain, 
            blueGain, 
            whiteGain,
            CurrentLookup[redGain],
            CurrentLookup[greenGain],
            CurrentLookup[blueGain],
            CurrentLookup[whiteGain])
    {
    }

    void Encode(uint8_t* encoded) const
    {
        // RGBW 4 bits each
        *encoded++ = operator[](V_IC_1) << 4 | operator[](V_IC_2);
        *encoded = operator[](V_IC_3) << 4 | operator[](V_IC_4);
    }

protected:
    static constexpr uint8_t CurrentLookup[16] = {
            18, 30, 41, 53, 64, 76, 87, 99,
            110, 133, 145, 156, 168, 179, 190 };
};

template <uint8_t V_IC_1, uint8_t V_IC_2, uint8_t V_IC_3, uint8_t V_IC_4>
class NeoSm16824eSettings : public NeoSm168x4SettingsBase
{
public:
    NeoSm16824eSettings(uint8_t redGain, uint8_t greenGain, uint8_t blueGain, uint8_t whiteGain, uint16_t resisterOhms) :
        NeoSm168x4SettingsBase(redGain,
            greenGain,
            blueGain,
            whiteGain,
            calcCurrent(resisterOhms, redGain),
            calcCurrent(resisterOhms, greenGain),
            calcCurrent(resisterOhms, blueGain),
            calcCurrent(resisterOhms, whiteGain)),
        extROhms(resisterOhms)
    {
    }

    void Encode(uint8_t* encoded) const
    {
        // RGBW 4 bits each
        *encoded++ = operator[](V_IC_1) << 4 | operator[](V_IC_2);
        *encoded = operator[](V_IC_3) << 4 | operator[](V_IC_4);
    }

protected:
    const uint16_t extROhms;

    static uint16_t calcCurrent(const uint16_t ohms, const uint8_t gain)
    {
        uint16_t mA = (1100 * (240 + (gain * 32)) / ohms); // from spec sheet, gain 0-15 instead
        return mA * 10; // return tenths of mA
    }

};

// CAUTION:  Make sure ColorIndex order for Neo4ByteFeature matches T_SETTINGS
template<typename T_SETTINGS> 
class NeoRgbwSm168x4Elements : 
    public Neo4ByteFeature<ColorIndexR, ColorIndexG, ColorIndexB, ColorIndexW>
{
public:
    typedef T_SETTINGS SettingsObject;
    static const size_t SettingsSize = 2;

    static void applySettings(MAYBE_UNUSED uint8_t* pData, MAYBE_UNUSED size_t sizeData, MAYBE_UNUSED const SettingsObject& settings)
    {
        // settings are at the end of the data stream
        uint8_t* pDest = pData + sizeData - SettingsSize;

        settings.Encode(pDest);
    }

    static uint8_t* pixels(MAYBE_UNUSED uint8_t* pData, MAYBE_UNUSED size_t sizeData)
    {
        return pData;
    }

    static const uint8_t* pixels(MAYBE_UNUSED const uint8_t* pData, MAYBE_UNUSED size_t sizeData)
    {
        return pData;
    }
};

// CAUTION:  Make sure ColorIndex order for Neo3ByteFeature matches T_SETTINGS
template<typename T_SETTINGS> class NeoRgbSm168x3Elements : 
    public Neo3ByteFeature<ColorIndexR, ColorIndexG, ColorIndexB>
{
public:
    typedef T_SETTINGS SettingsObject;
    static const size_t SettingsSize = 2;

    static void applySettings(MAYBE_UNUSED uint8_t* pData, MAYBE_UNUSED size_t sizeData, MAYBE_UNUSED const SettingsObject& settings)
    {
        // settings are at the end of the data stream
        uint8_t* pDest = pData + sizeData - SettingsSize;

        settings.Encode(pDest);
    }

    static uint8_t* pixels(MAYBE_UNUSED uint8_t* pData, MAYBE_UNUSED size_t sizeData)
    {
        return pData;
    }

    static const uint8_t* pixels(MAYBE_UNUSED const uint8_t* pData, MAYBE_UNUSED size_t sizeData)
    {
        return pData;
    }
};

// FIXME 2023-08-08 tonhuisman: Space added between >> to avoid compiler warning
typedef NeoRgbSm168x3Elements<NeoSm16803pbSettings<ColorIndexR, ColorIndexG, ColorIndexB> > NeoRgbSm16803pbFeature;
typedef NeoRgbSm168x3Elements<NeoSm16823eSettings<ColorIndexR, ColorIndexG, ColorIndexB> > NeoRgbSm16823eFeature;
typedef NeoRgbwSm168x4Elements<NeoSm16804ebSettings<ColorIndexR, ColorIndexG, ColorIndexB, ColorIndexW> > NeoRgbwSm16804ebFeature;
typedef NeoRgbwSm168x4Elements<NeoSm16824eSettings<ColorIndexR, ColorIndexG, ColorIndexB, ColorIndexW> > NeoRgbwSm16824eFeature;


