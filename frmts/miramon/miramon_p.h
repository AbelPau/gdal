/******************************************************************************
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Private class declarations for the MiraMonRaster classes used
 *           to read MiraMon (.img) files.  Public (C callable) declarations
 *           are in miramon.h.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMR_P_H_INCLUDED
#define MMR_P_H_INCLUDED

#include "cpl_port.h"
#include "miramon.h"

#include <cstdio>
#include <memory>
#include <vector>
#include <array>
#include <set>

#include "cpl_error.h"
#include "cpl_vsi.h"
#include "ogr_spatialref.h"

#ifdef CPL_LSB
#define MMRStandard(n, p)                                                      \
    {                                                                          \
    }
#else
void MMRStandard(int, void *);
#endif

#include "miramon.h"
#include "miramonrel.h"

#ifdef MSVC
#include "..\miramon_common\mm_gdal_constants.h"
#else
#include "../miramon_common/mm_gdal_constants.h"
#endif

class MMRBand;
class MMRDictionary;
class MMRSpillFile;
class MMRType;

constexpr auto pszExtRaster = ".img";
constexpr auto pszExtRasterREL = "I.rel";

/************************************************************************/
/*      Flag indicating read/write, or read-only access to data.        */
/************************************************************************/
enum class MMRAccess
{
    /*! Read only (no update) access */ MMR_ReadOnly = 0,
    /*! Read/write access. */ MMR_Update = 1
};

/************************************************************************/
/*                              MMRInfo_t                               */
/*                                                                      */
/*      This is just a structure, and used hold info about the whole    */
/*      dataset within miramonopen.cpp                                  */
/************************************************************************/
struct mmrinfo
{
    VSILFILE *fp = nullptr;

    CPLString osRELFileName;
    MMRRel *fRel = nullptr;  // Access stuff to REL file

    int nXSize;
    int nYSize;

    // List of rawBandNames in a subdataset
    std::vector<CPLString> papoSDSBands;

    int nBands;
    MMRBand **papoBand = nullptr;
};

const char *const *GetMMRAuxMetaDataList();

/************************************************************************/
/*                               MMRBand                                */
/************************************************************************/

class MMRBand
{
    VSILFILE *pfIMG;  // Point to IMG file
    MMRRel *pfRel;    // Rel where metadata is readed from

    int nBlocks;

    // indexed-RLE format
    std::vector<vsi_l_offset> aFileOffsets;

    // Palette info
    std::array<std::vector<double>, 4> aadfPaletteColors;

    int nNoDataOriginalIndex;
    bool bPaletteHasNodata;

    std::array<std::vector<double>, 4> aadfPCT;
    int nNoDataPaletteIndex;

    // Assigned Subdataset for this band.
    int nAssignedSDS;

    // Section in REL file that give information about the band
    CPLString osBandSection;
    // REL file name that contains the band
    CPLString osRELFileName;
    // File name relative to REL file with banda data
    CPLString osRawBandFileName;
    // Friendly osRawBandFileName
    CPLString osBandFileName;
    // Name of the band documented in REL metadata file.
    CPLString osBandName;
    // Descripcion of the band, not the name
    CPLString osFriendlyDescription;

    MMDataType eMMDataType;
    MMBytesPerPixel eMMBytesPerPixel;
    bool bIsCompressed;

    // Min and Max values from metadata:  This value should correspond
    // to the actual minimum and maximum, not to an approximation.
    // However, MiraMon is proof to approximate values. The minimum
    // and maximum values are useful, for example, to properly scale
    // colors, etc.
    bool bMinSet;
    double dfMin;
    bool bMaxSet;
    double dfMax;
    //
    bool bMinVisuSet;
    double dfVisuMin;  // Key Color_ValorColor_0 in COLOR_TEXT
    bool bMaxVisuSet;
    double dfVisuMax;  // Key Color_ValorColor_n_1 COLOR_TEXT

    CPLString pszRefSystem;

    // Extent values of the band:
    // They always refer to extreme outer coordinates,
    // not to cell centers.
    double dfBBMinX;
    double dfBBMinY;
    double dfBBMaxX;
    double dfBBMaxY;

    // Resolution of the pixel
    double nResolution;

  public:
    MMRBand(MMRInfo_t *, const char *pszSection);
    ~MMRBand();

    int Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        const char *pszSection, const char *pszKey, int *nValue,
        const char *pszErrorMessage);
    int GetDataTypeFromREL(const char *pszSection);
    int GetResolutionFromREL(const char *pszSection);
    int GetColumnsNumberFromREL(const char *pszSection);
    int GetRowsNumberFromREL(const char *pszSection);
    void GetNoDataValue(const char *pszSection);
    void GetNoDataDefinitionFromREL(const char *pszSection);
    void GetBoundingBoxFromREL(const char *pszSection);
    void GetReferenceSystemFromREL();
    void GetMinMaxValuesFromREL(const char *pszSection);
    void GetMinMaxVisuValuesFromREL(const char *pszSection);
    void GetFriendlyDescriptionFromREL(const char *pszSection);

    const std::vector<double> &GetPCT_Red() const
    {
        return aadfPCT[0];
    }

    const std::vector<double> &GetPCT_Green() const
    {
        return aadfPCT[1];
    }

    const std::vector<double> &GetPCT_Blue() const
    {
        return aadfPCT[2];
    }

    const std::vector<double> &GetPCT_Alpha() const
    {
        return aadfPCT[3];
    }

    int GetAssignedSubDataSet()
    {
        return nAssignedSDS;
    }

    void AssignSubDataSet(int nAssignedSDSIn)
    {
        nAssignedSDS = nAssignedSDSIn;
    }

    const CPLString &GetBandName() const
    {
        return osBandName;
    }

    const CPLString &GetBandSection() const
    {
        return osBandSection;
    }

    const CPLString &GetRELFileName() const
    {
        return osRELFileName;
    }

    void SetRELFileName(CPLString osRELFileNameIn)
    {
        osRELFileName = osRELFileNameIn;
    }

    const CPLString &GetRawBandFileName() const
    {
        return osRawBandFileName;
    }

    const CPLString &GetFriendlyDescription() const
    {
        return osFriendlyDescription;
    }

    MMDataType GeteMMDataType()
    {
        return eMMDataType;
    }

    MMBytesPerPixel GeteMMBytesPerPixel()
    {
        return eMMBytesPerPixel;
    }

    bool GetMinSet()
    {
        return bMinSet;
    }

    double GetMin()
    {
        return dfMin;
    }

    bool GetMaxSet()
    {
        return bMaxSet;
    }

    double GetMax()
    {
        return dfMax;
    }

    bool GetMinVisuSet()
    {
        return bMinVisuSet;
    }

    double GetVisuMin()
    {
        return dfVisuMin;
    }

    bool GetMaxVisuSet()
    {
        return bMaxVisuSet;
    }

    double GetVisuMax()
    {
        return dfVisuMax;
    }

    double GetBoundingBoxMinX()
    {
        return dfBBMinX;
    }

    double GetBoundingBoxMaxX()
    {
        return dfBBMaxX;
    }

    double GetBoundingBoxMinY()
    {
        return dfBBMinY;
    }

    double GetBoundingBoxMaxY()
    {
        return dfBBMaxY;
    }

    double GetPixelResolution()
    {
        return nResolution;
    }

    template <typename TYPE> CPLErr UncompressRow(void *rowBuffer);
    bool AcceptedDataType();
    CPLErr GetRowData(void *rowBuffer);
    int PositionAtStartOfRowOffsetsInFile();
    bool FillRowOffsets();
    CPLErr GetRasterBlock(int nXBlock, int nYBlock, void *pData, int nDataSize);

    void AssignRGBColor(int nIndexDstPalete, int nIndexSrcPalete);
    void AssignRGBColorDirectly(int nIndexDstPalete, double dfValue);
    CPLErr ConvertPaletteColors();
    CPLErr GetPCT();
    static CPLErr GetPaletteColors_DBF_Indexs(
        struct MM_DATA_BASE_XP &oColorTable, MM_EXT_DBF_N_FIELDS &nClauSimbol,
        MM_EXT_DBF_N_FIELDS &nRIndex, MM_EXT_DBF_N_FIELDS &nGIndex,
        MM_EXT_DBF_N_FIELDS &nBIndex);
    CPLErr GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF);
    CPLErr GetPaletteColors_PAL_P25_P65(CPLString os_Color_Paleta_DBF);

    MMRInfo_t *psInfo;

    int nBlockXSize;
    int nBlockYSize;

    int nWidth;   // Number of columns
    int nHeight;  // Number of rows

    int nBlocksPerRow;
    int nBlocksPerColumn;

    bool bNoDataSet;         // There is nodata?
    CPLString pszNodataDef;  // Definition of nodata
    double dfNoData;         // Value of nodata
};

#endif /* ndef MMR_P_H_INCLUDED */
