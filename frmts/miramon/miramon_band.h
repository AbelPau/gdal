/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRBand
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MM_BAND_INCLUDED
#define MM_BAND_INCLUDED

#include <vector>
#include <array>

#include "miramon_rel.h"

class MMRBand;

constexpr auto pszExtRaster = ".img";
constexpr auto pszExtRasterREL = "I.rel";
constexpr auto pszExtREL = ".rel";

/************************************************************************/
/*                               MMRBand                                */
/************************************************************************/

class MMRBand
{
  public:
    MMRBand(MMRInfo &, const char *pszSection);
    MMRBand(const MMRBand &) =
        delete;  // I don't want to construct a MMRBand from another MMRBand (effc++)
    MMRBand &operator=(const MMRBand &) =
        delete;  // I don't want to assing a MMRBand to another MMRBand (effc++)
    ~MMRBand();

    int Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        const CPLString osSection, const char *pszKey, int *nValue,
        const char *pszErrorMessage);
    int UpdateDataTypeFromREL(const CPLString osSection);
    void UpdateResolutionFromREL(const CPLString osSection);
    void UpdateResolutionYFromREL(const CPLString osSection);
    int UpdateColumnsNumberFromREL(const CPLString osSection);
    int UpdateRowsNumberFromREL(const CPLString osSection);
    void UpdateNoDataValue(const CPLString osSection);
    void UpdateNoDataDefinitionFromREL(const CPLString osSection);
    void UpdateBoundingBoxFromREL(const CPLString osSection);
    void UpdateReferenceSystemFromREL();
    void UpdateMinMaxValuesFromREL(const CPLString osSection);
    void UpdateMinMaxVisuValuesFromREL(const CPLString osSection);
    void UpdateFriendlyDescriptionFromREL(const CPLString osSection);
    template <typename TYPE> CPLErr UncompressRow(void *rowBuffer);
    CPLErr GetBlockData(void *rowBuffer);
    int PositionAtStartOfRowOffsetsInFile();
    bool FillRowOffsets();
    CPLErr GetRasterBlock(int nXBlock, int nYBlock, void *pData, int nDataSize);

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

    MMDataType GeteMMDataType() const
    {
        return eMMDataType;
    }

    MMBytesPerPixel GeteMMBytesPerPixel() const
    {
        return eMMBytesPerPixel;
    }

    bool GetMinSet() const
    {
        return bMinSet;
    }

    double GetMin() const
    {
        return dfMin;
    }

    bool GetMaxSet() const
    {
        return bMaxSet;
    }

    double GetMax() const
    {
        return dfMax;
    }

    bool GetMinVisuSet() const
    {
        return bMinVisuSet;
    }

    double GetVisuMin() const
    {
        return dfVisuMin;
    }

    bool GetMaxVisuSet() const
    {
        return bMaxVisuSet;
    }

    double GetVisuMax() const
    {
        return dfVisuMax;
    }

    double GetBoundingBoxMinX() const
    {
        return dfBBMinX;
    }

    double GetBoundingBoxMaxX() const
    {
        return dfBBMaxX;
    }

    double GetBoundingBoxMinY() const
    {
        return dfBBMinY;
    }

    double GetBoundingBoxMaxY() const
    {
        return dfBBMaxY;
    }

    double GetPixelResolution() const
    {
        return dfResolution;
    }

    double GetPixelResolutionY() const
    {
        return dfResolutionY;
    }

    bool BandHasNoData() const
    {
        return bNoDataSet;
    }

    double GetNoDataValue() const
    {
        return dfNoData;
    }

    MMRInfo *hMMR = nullptr;  // Just a pointer. No need to be freed

    int nBlockXSize;
    int nBlockYSize;

    int nWidth;   // Number of columns
    int nHeight;  // Number of rows

  private:
    VSILFILE *pfIMG;  // Point to IMG file
    MMRRel *pfRel;    // Rel where metadata is readed from

    int nBlocksPerRow;
    int nBlocksPerColumn;

    // indexed-RLE format
    std::vector<vsi_l_offset> aFileOffsets{};

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
    // These values will be dfMin/dfMax if they don't exist in REL file
    bool bMinVisuSet;
    double dfVisuMin;  // Key Color_ValorColor_0 in COLOR_TEXT
    bool bMaxVisuSet;
    double dfVisuMax;  // Key Color_ValorColor_n_1 COLOR_TEXT

    CPLString osRefSystem;

    // Extent values of the band:
    // They always refer to extreme outer coordinates,
    // not to cell centers.

    double dfBBMinX;
    double dfBBMinY;
    double dfBBMaxX;
    double dfBBMaxY;

    // Resolution of the pixel
    double dfResolution;
    double dfResolutionY;

    // Determines if dfResolution and dfResolutionY have been found
    // If is not found 1 is the defect value
    bool bSetResolution;

    // Nodata stuff
    bool bNoDataSet;        // There is nodata?
    CPLString osNodataDef;  // Definition of nodata
    double dfNoData;        // Value of nodata
};

#endif /* ndef MM_BAND_INCLUDED */
