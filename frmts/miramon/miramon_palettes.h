/******************************************************************************
 *
 * Project:  MiraMon Raster Driver
 * Purpose:  Implements MMRPalettes class 
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMRPALETTES_H_INCLUDED
#define MMRPALETTES_H_INCLUDED

#include <cstddef>
#include <vector>
#include <optional>
#include <array>

//#include "gdal_pam.h"
#include "gdal_rat.h"

#include "../miramon_common/mm_gdal_constants.h"  // For MM_EXT_DBF_N_FIELDS

/* ==================================================================== */
/*                            MMRPalettes                             */
/* ==================================================================== */

class MMRPalettes
{
  public:
    MMRPalettes(MMRRel &fRel, CPLString osBandSectionIn);
    MMRPalettes(const MMRPalettes &) =
        delete;  // I don't want to construct a MMRPalettes from another MMRBand (effc++)
    MMRPalettes &operator=(const MMRPalettes &) =
        delete;  // I don't want to assing a MMRPalettes to another MMRBand (effc++)
    ~MMRPalettes();

    void AssignColorFromDBF(struct MM_DATA_BASE_XP &oColorTable,
                            char *pzsRecord, char *pzsField,
                            MM_EXT_DBF_N_FIELDS &nRIndex,
                            MM_EXT_DBF_N_FIELDS &nGIndex,
                            MM_EXT_DBF_N_FIELDS &nBIndex, int nIPaletteIndex);

    bool IsValid() const
    {
        return bIsValid;
    }

    void SetIsValid(bool bIsValidIn)
    {
        bIsValid = bIsValidIn;
    }

    bool IsCategorical() const
    {
        return bIsCategorical;
    }

    void SetIsCategorical(bool bIsCategoricalIn)
    {
        bIsCategorical = bIsCategoricalIn;
    }

    bool IsIsConstantColor() const
    {
        return bIsConstantColor;
    }

    void SetIsConstantColor(bool bIsConstantColorIn)
    {
        bIsConstantColor = bIsConstantColorIn;
    }

    GDALColorEntry GetDefaultColorRGB() const
    {
        return sDefaultColorRGB;
    }

    GDALColorEntry GetConstantColorRGB() const
    {
        return sConstantColorRGB;
    }

    void SetConstantColorRGB(GDALColorEntry sConstantColorRGBIn)
    {
        sConstantColorRGB = sConstantColorRGBIn;
    }

    void SetConstantColorRGB(short c1, short c2, short c3)
    {
        sConstantColorRGB.c1 = c1;
        sConstantColorRGB.c2 = c2;
        sConstantColorRGB.c3 = c3;
    }

    void SetConstantColorRGB(short c1, short c2, short c3, short c4)
    {
        sConstantColorRGB.c1 = c1;
        sConstantColorRGB.c2 = c2;
        sConstantColorRGB.c3 = c3;
        sConstantColorRGB.c4 = c4;
    }

    bool HasNodata() const
    {
        return bHasNodata;
    }

    void SetHasNodata(bool bHasNodataIn)
    {
        bHasNodata = bHasNodataIn;
    }

    int GetNoDataPaletteIndex() const
    {
        return nNoDataPaletteIndex;
    }

    void SetNoDataPaletteIndex(bool nNoDataPaletteIndexIn)
    {
        nNoDataPaletteIndex = nNoDataPaletteIndexIn;
    }

    std::array<std::vector<double>, 4> aadfPaletteColors{};

  private:
    static CPLErr GetPaletteColors_DBF_Indexs(
        struct MM_DATA_BASE_XP &oColorTable, MM_EXT_DBF_N_FIELDS &nClauSimbol,
        MM_EXT_DBF_N_FIELDS &nRIndex, MM_EXT_DBF_N_FIELDS &nGIndex,
        MM_EXT_DBF_N_FIELDS &nBIndex);
    CPLErr GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF);
    CPLErr GetPaletteColors_PAL_P25_P65(CPLString os_Color_Paleta_DBF);

    bool bPaletteColorsRead = false;
    bool bIsCategorical = false;

    // Palette info
    GDALColorEntry sDefaultColorRGB = {0, 0, 0, 127};

    bool bHasNodata = false;
    // index in the DBF that gives nodata color
    int nNoDataPaletteIndex = 0;
    // Default color for nodata
    GDALColorEntry sNoDataColorRGB = {0, 0, 0, 0};

    bool bIsConstantColor = false;
    GDALColorEntry sConstantColorRGB = {0, 0, 0, 0};

    MMRRel *pfRel = nullptr;  // Rel where metadata is readed from
    CPLString osBandSection;
    bool bIsValid = false;  // Determines if the created object is valid or not.
};

#endif  // MMRPALETTES_H_INCLUDED
