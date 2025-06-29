/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Contains generic raster functions 
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#include <algorithm>

#include "miramon_p.h"
#include "miramon_rastertools.h"  // For MMRGetFileNameFromRelName()

#ifdef MSVC
#include "..\miramon_common\mm_gdal_functions.h"  // For MM_ReadExtendedDBFHeaderFromFile()
#else
#include "../miramon_common/mm_gdal_functions.h"  // For MM_ReadExtendedDBFHeaderFromFile()
#endif

/************************************************************************/
/*                              MMRBand()                               */
/************************************************************************/
// [ATTRIBUTE_DATA:xxxx] or [OVERVIEW:ASPECTES_TECNICS]
int MMRBand::Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
    const char *pszSection, const char *pszKey, int *nValue,
    const char *pszErrorMessage)
{
    if (!pszSection || !pszKey || !nValue)
        return 1;

    CPLString osValue =
        pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, pszSection, pszKey);

    if (osValue.empty())
    {
        osValue = pfRel->GetMetadataValue(SECTION_OVERVIEW,
                                          SECTION_ASPECTES_TECNICS, pszKey);
        if (osValue.empty())
        {
            if (pszErrorMessage)
                CPLError(CE_Failure, CPLE_AppDefined, "%s", pszErrorMessage);
            return 1;
        }
    }
    *nValue = osValue.empty() ? 0 : atoi(osValue);
    return 0;
}

// Getting data type from metadata
int MMRBand::GetDataTypeFromREL(const char *pszSection)
{
    CPLString osValue = pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA,
                                                pszSection, "TipusCompressio");

    eMMDataType = MMDataType::DATATYPE_AND_COMPR_UNDEFINED;
    eMMBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_UNDEFINED;

    if (osValue.empty() ||
        pfRel->GetDataTypeAndBytesPerPixel(osValue.c_str(), &eMMDataType,
                                           &eMMBytesPerPixel) == 1)
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MMRBand::MMRBand : No nDataType documented");
        return 1;
    }

    if (!AcceptedDataType())
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MMRBand::GetDataTypeFromREL data type unhandled");
        return 1;
    }
    return 0;
}

// Getting number of columns from metadata
int MMRBand::GetResolutionFromREL(const char *pszSection)
{
    CPLString osValue = pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA,
                                                pszSection, "resolution");
    if (osValue.empty())
    {
        osValue = pfRel->GetMetadataValue(SECTION_SPATIAL_REFERENCE_SYSTEM,
                                          SECTION_HORIZONTAL, "resolution");
        if (osValue.empty())
        {
            nWidth = 0;
            nHeight = 0;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "MMRBand::MMRBand : No resolution documented");
            return 1;
        }
    }
    nResolution = osValue.empty() ? 0 : atoi(osValue);
    return 0;
}

// Getting number of columns from metadata
int MMRBand::GetColumnsNumberFromREL(const char *pszSection)
{
    return Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        pszSection, "columns", &nWidth,
        "MMRBand::MMRBand : No number of columns documented");
}

int MMRBand::GetRowsNumberFromREL(const char *pszSection)
{
    return Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        pszSection, "rows", &nHeight,
        "MMRBand::MMRBand : No number of rows documented");
}

// Getting nodata value from metadata
void MMRBand::GetNoDataValue(const char *pszSection)
{
    CPLString osValue =
        pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, pszSection, "NODATA");
    if (osValue.empty())
    {
        dfNoData = 0;  // No a valid value.
        bNoDataSet = false;
    }
    else
    {
        dfNoData = atoi(osValue);
        bNoDataSet = true;
    }
}

// Getting nodata value from metadata
void MMRBand::GetNoDataDefinitionFromREL(const char *pszSection)
{
    if (!bNoDataSet)
        return;

    pszNodataDef = pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, pszSection,
                                           "NODATADef");
}

void MMRBand::GetMinMaxValuesFromREL(const char *pszSection)
{
    bMinSet = false;

    CPLString osValue =
        pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, pszSection, "min");
    if (!osValue.empty())
    {
        bMinSet = true;
        dfMin = atof(osValue);
    }

    bMaxSet = false;
    osValue =
        pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, pszSection, "max");

    if (!osValue.empty())
    {
        bMaxSet = true;
        dfMax = atof(osValue);
    }
}

void MMRBand::GetMinMaxVisuValuesFromREL(const char *pszSection)
{
    bMinVisuSet = false;
    dfVisuMin = 1;

    CPLString osValue = pfRel->GetMetadataValue(SECTION_COLOR_TEXT, pszSection,
                                                "Color_ValorColor_0");
    if (!osValue.empty())
    {
        bMinVisuSet = true;
        dfVisuMin = atof(osValue);
    }

    bMaxVisuSet = false;
    dfVisuMax = 1;

    osValue = pfRel->GetMetadataValue(SECTION_COLOR_TEXT, pszSection,
                                      "Color_ValorColor_n_1");

    if (!osValue.empty())
    {
        bMaxVisuSet = true;
        dfVisuMax = atof(osValue);
    }
}

void MMRBand::GetFriendlyDescriptionFromREL(const char *pszSection)
{
    osFriendlyDescription = pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA,
                                                    pszSection, "descriptor");
}

void MMRBand::GetReferenceSystemFromREL()
{
    pszRefSystem = pfRel->GetMetadataValue(
        "SPATIAL_REFERENCE_SYSTEM:HORIZONTAL", "HorizontalSystemIdentifier");
}

void MMRBand::GetBoundingBoxFromREL(const char *pszSection)
{
    // Bounding box of the band
    // [ATTRIBUTE_DATA:xxxx:EXTENT] or [EXTENT]
    CPLString osValue = pfRel->GetMetadataValue(
        SECTION_ATTRIBUTE_DATA, pszSection, SECTION_EXTENT, "MinX");
    if (osValue.empty())
        dfBBMinX = 0;
    else
        dfBBMinX = atof(osValue);

    osValue = pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, pszSection,
                                      SECTION_EXTENT, "MaxX");
    if (osValue.empty())
        dfBBMaxX = nWidth;
    else
        dfBBMaxX = atof(osValue);

    osValue = pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, pszSection,
                                      SECTION_EXTENT, "MinY");
    if (osValue.empty())
        dfBBMinY = 0;
    else
        dfBBMinY = atof(osValue);

    osValue = pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, pszSection,
                                      SECTION_EXTENT, "MaxY");
    if (osValue.empty())
        dfBBMaxY = nHeight;
    else
        dfBBMaxY = atof(osValue);
}

MMRBand::MMRBand(MMRInfo_t *psInfoIn, const char *pszSection)
    : pfIMG(nullptr), pfRel(psInfoIn->fRel), nBlocks(0),
      nNoDataOriginalIndex(0), bPaletteHasNodata(false), nNoDataPaletteIndex(0),
      nAssignedSDS(0), osBandSection(pszSection), osRELFileName(""),
      osRawBandFileName(""), osBandFileName(""), osBandName(""),
      osFriendlyDescription(""), eMMDataType(static_cast<MMDataType>(
                                     MMDataType::DATATYPE_AND_COMPR_UNDEFINED)),
      eMMBytesPerPixel(static_cast<MMBytesPerPixel>(
          MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_UNDEFINED)),
      bIsCompressed(false), bMinSet(false), dfMin(0.0), bMaxSet(false),
      dfMax(0.0), bMinVisuSet(false), dfVisuMin(0.0), bMaxVisuSet(false),
      dfVisuMax(0.0), pszRefSystem(""), dfBBMinX(0), dfBBMinY(0), dfBBMaxX(0),
      dfBBMaxY(0), nResolution(0), psInfo(psInfoIn),
      /*eDataType(static_cast<EPTType>(EPT_MIN)), poNode(nullptr), */
      nBlockXSize(0), nBlockYSize(1), nWidth(psInfoIn->nXSize),
      nHeight(psInfo->nYSize), nBlocksPerRow(1), nBlocksPerColumn(1),
      bNoDataSet(false), pszNodataDef(""), dfNoData(0.0)
{
    // Getting band and band file name from metadata
    osRawBandFileName = pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA,
                                                pszSection, KEY_NomFitxer);

    if (osRawBandFileName.empty())
    {
        osBandFileName = MMRGetFileNameFromRelName(psInfoIn->osRELFileName);
        if (osBandFileName.empty())
        {
            nWidth = 0;
            nHeight = 0;
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "The REL file '%s' contains a documented \
                band with no explicit name. Section [%s] or [%s:%s].\n",
                     psInfo->osRELFileName.c_str(), SECTION_ATTRIBUTE_DATA,
                     SECTION_ATTRIBUTE_DATA, pszSection);
            return;
        }
        osBandName = CPLGetBasenameSafe(osBandFileName);
        osRawBandFileName = osBandName;
    }
    else
    {
        osBandName = CPLGetBasenameSafe(osRawBandFileName);
        CPLString osAux =
            CPLGetPathSafe(static_cast<const char *>(pfRel->GetRELNameChar()));
        osBandFileName =
            CPLFormFilenameSafe(osAux.c_str(), osRawBandFileName.c_str(), "");
    }

    // There is a band file documented?
    if (osBandName.empty())
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "The REL file '%s' contains a documented \
            band with no explicit name. Section [%s] or [%s:%s].\n",
                 psInfo->osRELFileName.c_str(), SECTION_ATTRIBUTE_DATA,
                 SECTION_ATTRIBUTE_DATA, pszSection);
        return;
    }

    // Getting essential metadata documented at
    // https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf

    // Getting number of columns and rows
    if (GetColumnsNumberFromREL(pszSection))
    {
        nWidth = 0;
        nHeight = 0;
        return;
    }

    if (GetRowsNumberFromREL(pszSection))
    {
        nWidth = 0;
        nHeight = 0;
        return;
    }

    if (nWidth <= 0 || nHeight <= 0)
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MMRBand::MMRBand : (nWidth <= 0 || nHeight <= 0)");
        return;
    }
    else
    {
        psInfoIn->nXSize = nWidth;
        psInfoIn->nYSize = nHeight;
    }

    // Getting data type and compression
    if (GetDataTypeFromREL(pszSection))
        return;

    // Let's see if there is RLE compression
    bIsCompressed =
        (((eMMDataType >= MMDataType::DATATYPE_AND_COMPR_BYTE_RLE) &&
          (eMMDataType <= MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE)) ||
         eMMDataType == MMDataType::DATATYPE_AND_COMPR_BIT);

    // Getting resolution
    if (GetResolutionFromREL(pszSection))
        return;

    // Getting min and max values
    GetMinMaxValuesFromREL(pszSection);

    // Getting min and max values for simbolization
    GetMinMaxVisuValuesFromREL(pszSection);
    if (!bMinVisuSet)
    {
        if (bMinSet)
            dfVisuMin = dfMin;
    }
    if (!bMaxVisuSet)
    {
        if (bMaxSet)
            dfVisuMax = dfMax;
    }

    // Getting the friendly description of the band
    GetFriendlyDescriptionFromREL(pszSection);

    // Getting NoData value and definition
    GetNoDataValue(pszSection);
    GetNoDataDefinitionFromREL(
        pszSection);  // ·$·TODO put it in metadata MIRAMON subdomain?

    // Getting reference system and coordinates of the geographic bounding box
    GetReferenceSystemFromREL();

    // Getting the bounding box: coordinates in the terrain
    GetBoundingBoxFromREL(pszSection);

    // MiraMon IMG files are efficient in going to an specified row.
    // So le'ts configurate the blocks as line blocks.
    nBlocks = nHeight;
    nBlockXSize = nWidth;
    nBlockYSize = 1;
    nBlocksPerRow = 1;
    nBlocksPerColumn = nHeight;

    // Can the binary file that contains all data for this band be opened?
    pfIMG = VSIFOpenL(osBandFileName, "rb");
    if (!pfIMG)
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open MiraMon band file `%s' with access 'rb'.",
                 osBandFileName.c_str());
        return;
    }
}

/************************************************************************/
/*                              ~MMRBand()                              */
/************************************************************************/

MMRBand::~MMRBand()

{
    if (pfIMG != nullptr)
        CPL_IGNORE_RET_VAL(VSIFCloseL(pfIMG));
}

template <typename TYPE> CPLErr MMRBand::UncompressRow(void *rowBuffer)
{
    int acumulat = 0L, ii = 0L;
    unsigned char comptador;

    TYPE valor_rle;
    while (acumulat < nWidth)
    {
        if (VSIFReadL(&comptador, sizeof(comptador), 1, pfIMG) != 1)
            return CE_Failure;

        if (comptador == 0) /* Not compressed part */
        {
            /* The following counter read does not indicate
            "how many repeated values follow" but rather
            "how many are decompressed in standard raster format" */
            if (VSIFReadL(&comptador, sizeof(comptador), 1, pfIMG) != 1)
                return CE_Failure;
            acumulat += comptador;

            if (acumulat > nWidth) /* This should not happen if the file
                                  is RLE and does not share counters across rows */
                return CE_Failure;

            for (; ii < acumulat; ii++)
            {
                VSIFReadL(&valor_rle, sizeof(TYPE), 1, pfIMG);
                memcpy((static_cast<TYPE *>(rowBuffer)) + ii, &valor_rle,
                       sizeof(TYPE));
            }
        }
        else
        {
            acumulat += comptador;

            if (acumulat > nWidth) /* This should not happen if the file
                                  is RLE and does not share counters across rows */
                return CE_Failure;

            if (VSIFReadL(&valor_rle, sizeof(TYPE), 1, pfIMG) != 1)
                return CE_Failure;
            for (; ii < acumulat; ii++)
                memcpy((static_cast<TYPE *>(rowBuffer)) + ii, &valor_rle,
                       sizeof(TYPE));
        }
    }

    return CE_None;
}

bool MMRBand::AcceptedDataType()
{
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_BIT)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_BYTE)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_INTEGER)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_UINTEGER)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_LONG)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_REAL)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_DOUBLE)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_BYTE_RLE)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_LONG_RLE)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_REAL_RLE)
        return true;
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE)
        return true;

    return false;
}

/************************************************************************/
/*                 GetRowData                             */
/************************************************************************/
CPLErr MMRBand::GetRowData(void *rowBuffer)
{
    const int nDataTypeSizeBytes =
        std::max(1, static_cast<int>(eMMBytesPerPixel));
    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_BIT)
    {
        const int nGDALBlockSize = static_cast<int>(ceil(nBlockXSize / 8.0));

        if (VSIFReadL(rowBuffer, nGDALBlockSize, 1, pfIMG) != 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "\nError while reading band");
            return CE_Failure;
        }
        return CE_None;
    }

    if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_BYTE ||
        eMMDataType == MMDataType::DATATYPE_AND_COMPR_INTEGER ||
        eMMDataType == MMDataType::DATATYPE_AND_COMPR_UINTEGER ||
        eMMDataType == MMDataType::DATATYPE_AND_COMPR_LONG ||
        eMMDataType == MMDataType::DATATYPE_AND_COMPR_REAL ||
        eMMDataType == MMDataType::DATATYPE_AND_COMPR_DOUBLE)
    {
        if (VSIFReadL(rowBuffer, nDataTypeSizeBytes, nWidth, pfIMG) !=
            static_cast<size_t>(nWidth))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "\nError while reading band");
            return CE_Failure;
        }
        return CE_None;
    }

    CPLErr peErr;
    switch (eMMDataType)
    {
        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
            peErr = UncompressRow<GByte>(rowBuffer);
            break;
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
            peErr = UncompressRow<GInt16>(rowBuffer);
            break;
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
            peErr = UncompressRow<GUInt16>(rowBuffer);
            break;
        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
            peErr = UncompressRow<GInt32>(rowBuffer);
            break;
        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
            peErr = UncompressRow<float>(rowBuffer);
            break;
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:
            peErr = UncompressRow<double>(rowBuffer);
            break;

        default:
            CPLError(CE_Failure, CPLE_AppDefined, "\nError in datatype");
            peErr = CE_Failure;
    }

    return peErr;
}  // End of GetRowData()

/************************************************************************/
/*                 PositionAtStartOfRowOffsetsInFile                */
/************************************************************************/
int MMRBand::PositionAtStartOfRowOffsetsInFile()
{
    vsi_l_offset nFileSize, nHeaderOffset;
    char szChain[16];
    short int nVersion, nSubVersion;
    int nOffsetSize, nOffsetsSectionType;

    if (VSIFSeekL(pfIMG, 0, SEEK_END))
        return 0;

    if (static_cast<vsi_l_offset>(-1) == (nFileSize = VSIFTellL(pfIMG)))
        return 0;

    if (nHeight)
    {
        if (nFileSize < static_cast<vsi_l_offset>(32) + nHeight + 32)
            return 0;
    }

    if (VSIFSeekL(pfIMG, -32, SEEK_CUR))  // Reading final header.
        return 0;
    if (VSIFReadL(szChain, 16, 1, pfIMG) != 1)
        return 0;
    for (int nIndex = 0; nIndex < 16; nIndex++)
    {
        if (szChain[nIndex] != '\0')
            return 0;  // Supposed 0's are not 0.
    }

    if (VSIFReadL(szChain, 8, 1, pfIMG) != 1)
        return 0;

    if (strncmp(szChain, "IMG ", 4) || szChain[5] != '.')
        return 0;

    // Some version checks
    szChain[7] = 0;
    if (sscanf(szChain + 6, "%hd", &nSubVersion) != 1 || nSubVersion < 0)
        return 0;

    szChain[5] = 0;
    if (sscanf(szChain + 4, "%hd", &nVersion) != 1 || nVersion != 1)
        return 0;

    // Next header to be read
    if (VSIFReadL(&nHeaderOffset, sizeof(vsi_l_offset), 1, pfIMG) != 1)
        return 0;

    int bRepeat;
    do
    {
        bRepeat = FALSE;

        if (VSIFSeekL(pfIMG, nHeaderOffset, SEEK_SET))
            return 0;

        if (VSIFReadL(szChain, 8, 1, pfIMG) != 1)
            return 0;

        if (strncmp(szChain, "IMG ", 4) || szChain[5] != '.')
            return 0;

        if (VSIFReadL(&nOffsetsSectionType, 4, 1, pfIMG) != 1)
            return 0;

        if (nOffsetsSectionType != 2)  // 2 = row offsets section
        {
            // This is not the section I am looking for
            if (VSIFSeekL(pfIMG, 8 + 4, SEEK_CUR))
                return 0;

            if (VSIFReadL(&nHeaderOffset, sizeof(vsi_l_offset), 1, pfIMG) != 1)
                return 0;

            if (nHeaderOffset == 0)
                return 0;

            bRepeat = TRUE;
        }

    } while (bRepeat);

    szChain[7] = 0;
    if (sscanf(szChain + 6, "%hd", &nSubVersion) != 1 || nSubVersion < 0)
        return 0;
    szChain[5] = 0;
    if (sscanf(szChain + 4, "%hd", &nVersion) != 1 || nVersion != 1)
        return 0;

    /*
        Now I'm in the correct section
        -------------------------------
        Info about this section:
        RasterRLE: minumum size: nHeight*2
        Offsets:   minimum size: 32+nHeight*4
        Final:     size: 32
    */

    if (nHeight)
    {
        if (nHeaderOffset < static_cast<vsi_l_offset>(nHeight) *
                                2 ||  // Minumum size of an RLE
            nFileSize - nHeaderOffset <
                static_cast<vsi_l_offset>(32) + nHeight +
                    32)  // Minumum size of the section in version 1.0
            return 0;
    }

    if (VSIFReadL(&nOffsetSize, 4, 1, pfIMG) != 1 ||
        (nOffsetSize != 8 && nOffsetSize != 4 && nOffsetSize != 2 &&
         nOffsetSize != 1))
        return 0;

    if (nHeight)
    {
        if (nFileSize - nHeaderOffset <
            32 + static_cast<vsi_l_offset>(nOffsetSize) * nHeight +
                32)  // No space for this section in this file
            return 0;

        // I leave the file prepared to read offsets
        if (VSIFSeekL(pfIMG, 16, SEEK_CUR))
            return 0;
    }
    else
    {
        if (VSIFSeekL(pfIMG, 4, SEEK_CUR))
            return 0;

        if (VSIFSeekL(pfIMG, 4, SEEK_CUR))
            return 0;

        // I leave the file prepared to read offsets
        if (VSIFSeekL(pfIMG, 8, SEEK_CUR))
            return 0;
    }

    // There are offsets!
    return nOffsetSize;
}  // Fi de PositionAtStartOfRowOffsetsInFile()

/************************************************************************/
/*                              FillRowOffsets()                         */
/************************************************************************/
bool MMRBand::FillRowOffsets()
{
    vsi_l_offset nStartOffset;
    int nIRow;
    vsi_l_offset nBytesPerPixelPerNCol;
    int nSizeToRead;  // nSizeToRead is not an offset, but the size of the offsets being read
                      // directly from the IMG file (can be 1, 2, 4, or 8).
    vsi_l_offset nFileByte;
    size_t nMaxBytesPerCompressedRow;
    const int nGDALBlockSize = static_cast<int>(ceil(nBlockXSize / 8.0));
    ;

    // If it's filled, there is no need to fill it again
    if (aFileOffsets.size() > 0)
        return true;

    try
    {
        aFileOffsets.resize(static_cast<size_t>(nHeight) + 1);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    const int nDataTypeSizeBytes =
        std::max(1, static_cast<int>(eMMBytesPerPixel));
    switch (eMMDataType)
    {
        case MMDataType::DATATYPE_AND_COMPR_BIT:

            // "<=" it's ok. There is space and it's to make easier the programming
            for (nIRow = 0; nIRow <= nHeight; nIRow++)
                aFileOffsets[nIRow] =
                    static_cast<vsi_l_offset>(nIRow) * nGDALBlockSize;
            break;

        case MMDataType::DATATYPE_AND_COMPR_BYTE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER:
        case MMDataType::DATATYPE_AND_COMPR_LONG:
        case MMDataType::DATATYPE_AND_COMPR_REAL:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE:
            nBytesPerPixelPerNCol =
                nDataTypeSizeBytes * static_cast<vsi_l_offset>(nWidth);
            // "<=" it's ok. There is space and it's to make easier the programming
            for (nIRow = 0; nIRow <= nHeight; nIRow++)
                aFileOffsets[nIRow] =
                    static_cast<vsi_l_offset>(nIRow) * nBytesPerPixelPerNCol;
            break;

        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:

            nStartOffset = VSIFTellL(pfIMG);

            // Let's determine if are there offsets in the file
            if (0 < (nSizeToRead = PositionAtStartOfRowOffsetsInFile()))
            {
                // I have offsets!!
                nFileByte = 0L;  // all bits to 0
                for (nIRow = 0; nIRow < nHeight; nIRow++)
                {
                    if (VSIFReadL(&nFileByte, nSizeToRead, 1, pfIMG) != 1)
                        return false;

                    aFileOffsets[nIRow] = nFileByte;
                }
                aFileOffsets[nIRow] = 0;  // Not reliable
                VSIFSeekL(pfIMG, nStartOffset, SEEK_SET);
                break;
            }

            // Not indexed RLE. We create a dynamic indexation
            nMaxBytesPerCompressedRow =
                static_cast<int>(eMMBytesPerPixel)
                    ? (nWidth * (static_cast<int>(eMMBytesPerPixel) + 1))
                    : (nWidth * (1 + 1));
            unsigned char *pBuffer;

            if (nullptr == (pBuffer = static_cast<unsigned char *>(
                                VSI_MALLOC_VERBOSE(nMaxBytesPerCompressedRow))))
            {
                VSIFSeekL(pfIMG, nStartOffset, SEEK_SET);
                return false;
            }

            VSIFSeekL(pfIMG, 0, SEEK_SET);
            aFileOffsets[0] = 0;
            for (nIRow = 0; nIRow < nHeight; nIRow++)
            {
                GetRowData(pBuffer);
                aFileOffsets[static_cast<size_t>(nIRow) + 1] = VSIFTellL(pfIMG);
            }
            VSIFree(pBuffer);
            VSIFSeekL(pfIMG, nStartOffset, SEEK_SET);
            break;

        default:
            return false;
    }  // End of switch (eMMDataType)
    return true;

}  // End of FillRowOffsets()

/************************************************************************/
/*                           GetRasterBlock()                           */
/************************************************************************/

CPLErr MMRBand::GetRasterBlock(int nXBlock, int nYBlock, void *pData,
                               int nDataSize)

{
    const int iBlock = nXBlock + nYBlock * nBlocksPerRow;
    const int nDataTypeSizeBytes =
        std::max(1, static_cast<int>(eMMBytesPerPixel));
    const int nGDALBlockSize = nDataTypeSizeBytes * nBlockXSize * nBlockYSize;

    // Calculate block offset in case we have spill file. Use predefined
    // block map otherwise.
    if (!pfIMG)
    {
        CPLError(CE_Failure, CPLE_FileIO, "File band not opened: \n%s",
                 osBandFileName.c_str());
        return CE_Failure;
    }

    if (nDataSize != -1 && (nGDALBlockSize > INT_MAX ||
                            static_cast<int>(nGDALBlockSize) > nDataSize))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid block size: %d",
                 static_cast<int>(nGDALBlockSize));
        return CE_Failure;
    }

    // Getting the row offsets to optimize access.
    if (FillRowOffsets() == false || aFileOffsets.size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Some error in offsets calculation");
        return CE_Failure;
    }

    // Read the block in the documented or deduced offset
    if (VSIFSeekL(pfIMG, aFileOffsets[iBlock], SEEK_SET))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Read from invalid offset for grid block.");
        return CE_Failure;
    }

    return GetRowData(pData);
}

// Colors in a DBF format file
CPLErr MMRBand::GetPaletteColors_DBF_Indexs(struct MM_DATA_BASE_XP &oColorTable,
                                            MM_EXT_DBF_N_FIELDS &nClauSimbol,
                                            MM_EXT_DBF_N_FIELDS &nRIndex,
                                            MM_EXT_DBF_N_FIELDS &nGIndex,
                                            MM_EXT_DBF_N_FIELDS &nBIndex)
{
    nClauSimbol = oColorTable.nFields;
    nRIndex = oColorTable.nFields;
    nGIndex = oColorTable.nFields;
    nBIndex = oColorTable.nFields;

    for (MM_EXT_DBF_N_FIELDS nIField = 0; nIField < oColorTable.nFields;
         nIField++)
    {
        if (EQUAL(oColorTable.pField[nIField].FieldName, "CLAUSIMBOL"))
            nClauSimbol = nIField;
        else if (EQUAL(oColorTable.pField[nIField].FieldName, "R_COLOR"))
            nRIndex = nIField;
        else if (EQUAL(oColorTable.pField[nIField].FieldName, "G_COLOR"))
            nGIndex = nIField;
        else if (EQUAL(oColorTable.pField[nIField].FieldName, "B_COLOR"))
            nBIndex = nIField;
    }

    if (nClauSimbol == oColorTable.nFields || nRIndex == oColorTable.nFields ||
        nGIndex == oColorTable.nFields || nBIndex == oColorTable.nFields)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                 oColorTable.szFileName);
        return CE_Failure;
    }

    return CE_None;
}

// Colors in a DBF format file
CPLErr MMRBand::GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF)

{
    CPLString osAux =
        CPLGetPathSafe(static_cast<const char *>(pfRel->GetRELNameChar()));
    CPLString osColorTableFileName =
        CPLFormFilenameSafe(osAux.c_str(), os_Color_Paleta_DBF.c_str(), "");

    struct MM_DATA_BASE_XP oColorTable;
    memset(&oColorTable, 0, sizeof(oColorTable));

    if (MM_ReadExtendedDBFHeaderFromFile(
            osColorTableFileName.c_str(), &oColorTable,
            static_cast<const char *>(pfRel->GetRELNameChar())))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Error reading color table \"%s\".",
                 osColorTableFileName.c_str());
        return CE_None;
    }
    int nPCTColors = static_cast<int>(oColorTable.nRecords);  // Safe cast
    if (nPCTColors < 0 || nPCTColors > 65536)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid number of colors: %d",
                 nPCTColors);
        return CE_Failure;
    }

    MM_EXT_DBF_N_FIELDS nClauSimbol, nRIndex, nGIndex, nBIndex;
    if (CE_Failure == GetPaletteColors_DBF_Indexs(oColorTable, nClauSimbol,
                                                  nRIndex, nGIndex, nBIndex))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid name of color fields: %d", nPCTColors);
        return CE_Failure;
    }

    if (oColorTable.pField[nRIndex].BytesPerField == 0 ||
        oColorTable.pField[nGIndex].BytesPerField == 0 ||
        oColorTable.pField[nBIndex].BytesPerField == 0 ||
        oColorTable.pField[nRIndex].FieldType != 'N' ||
        oColorTable.pField[nGIndex].FieldType != 'N' ||
        oColorTable.pField[nBIndex].FieldType != 'N')
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                 osColorTableFileName.c_str());
        return CE_Failure;
    }

    VSIFSeekL(oColorTable.pfDataBase, oColorTable.FirstRecordOffset, SEEK_SET);

    MM_ACCUMULATED_BYTES_TYPE_DBF nBufferSize = oColorTable.BytesPerRecord + 1;
    char *pzsBuffer = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));
    char *pzsField = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));

    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPaletteColors[iColumn].resize(nPCTColors, 0);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    for (int nIRecord = 0; nIRecord < nPCTColors; nIRecord++)
    {
        if (oColorTable.BytesPerRecord !=
            VSIFReadL(pzsBuffer, sizeof(unsigned char),
                      oColorTable.BytesPerRecord, oColorTable.pfDataBase))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                     osColorTableFileName.c_str());
            return CE_Failure;
        }

        // Nodata identification
        memcpy(pzsField,
               pzsBuffer + oColorTable.pField[nClauSimbol].AccumulatedBytes,
               oColorTable.pField[nClauSimbol].BytesPerField);
        pzsField[oColorTable.pField[nClauSimbol].BytesPerField] = '\0';
        CPLString osField = pzsField;
        osField.replaceAll(" ", "");
        if (osField.empty())  // Nodata value
        {
            bPaletteHasNodata = true;
            nNoDataOriginalIndex = nIRecord;
        }

        // RED
        memcpy(pzsField,
               pzsBuffer + oColorTable.pField[nRIndex].AccumulatedBytes,
               oColorTable.pField[nRIndex].BytesPerField);
        pzsField[oColorTable.pField[nRIndex].BytesPerField] = '\0';
        osField.replaceAll(" ", "");
        aadfPaletteColors[0][nIRecord] = CPLAtof(pzsField);

        // GREEN
        memcpy(pzsField,
               pzsBuffer + oColorTable.pField[nGIndex].AccumulatedBytes,
               oColorTable.pField[nGIndex].BytesPerField);
        pzsField[oColorTable.pField[nGIndex].BytesPerField] = '\0';
        osField.replaceAll(" ", "");
        aadfPaletteColors[1][nIRecord] = CPLAtof(pzsField);

        // BLUE
        memcpy(pzsField,
               pzsBuffer + oColorTable.pField[nBIndex].AccumulatedBytes,
               oColorTable.pField[nBIndex].BytesPerField);
        pzsField[oColorTable.pField[nBIndex].BytesPerField] = '\0';
        osField.replaceAll(" ", "");
        aadfPaletteColors[2][nIRecord] = CPLAtof(pzsField);

        // ALPHA
        if (aadfPaletteColors[0][nIRecord] == -1 &&
            aadfPaletteColors[1][nIRecord] == -1 &&
            aadfPaletteColors[2][nIRecord] == -1)
        {
            // Transparent (white or whatever color)
            aadfPaletteColors[0][nIRecord] = 0;
            aadfPaletteColors[1][nIRecord] = 0;
            aadfPaletteColors[2][nIRecord] = 0;
            aadfPaletteColors[3][nIRecord] = 0;
        }
        else
            aadfPaletteColors[3][nIRecord] = 255;
    }

    VSIFree(pzsField);
    VSIFree(pzsBuffer);
    VSIFCloseL(oColorTable.pfDataBase);
    MM_ReleaseMainFields(&oColorTable);

    return CE_None;
}

// Colors in a PAL, P25 or P65 format files
CPLErr MMRBand::GetPaletteColors_PAL_P25_P65(CPLString os_Color_Paleta_DBF)

{
    CPLString osAux =
        CPLGetPathSafe(static_cast<const char *>(pfRel->GetRELNameChar()));
    CPLString osColorTableFileName =
        CPLFormFilenameSafe(osAux.c_str(), os_Color_Paleta_DBF.c_str(), "");

    // This kind of palette has not NoData color.
    //bPaletteHasNodata = false;

    CPLString osExtension = CPLGetExtensionSafe(os_Color_Paleta_DBF);
    int nNReadPaletteColors = 0;
    int nNPaletteColors = 0;

    if (osExtension.tolower() == "pal")
        nNPaletteColors = 64;
    else if (osExtension.tolower() == "p25")
        nNPaletteColors = 256;
    else if (osExtension.tolower() == "p65")
        nNPaletteColors = 65536;
    else
        return CE_None;

    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPaletteColors[iColumn].resize(nNPaletteColors, 0);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    VSILFILE *fpColorTable = VSIFOpenL(osColorTableFileName, "rt");
    if (!fpColorTable)
    {
        VSIFCloseL(fpColorTable);
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                 osColorTableFileName.c_str());
        return CE_Failure;
    }

    nNReadPaletteColors = 0;
    const char *pszLine;
    while ((pszLine = CPLReadLineL(fpColorTable)) != nullptr)
    {
        // Ignore empty lines
        if (pszLine[0] == '\0')
            continue;

        char **papszTokens = CSLTokenizeString2(pszLine, " \t", 0);
        if (CSLCount(papszTokens) != 4)
        {
            VSIFCloseL(fpColorTable);
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                     osColorTableFileName.c_str());
            return CE_Failure;
        }

        // Index of the color
        // papszTokens[0] is ignored;

        // RED
        aadfPaletteColors[0][nNReadPaletteColors] = CPLAtof(papszTokens[1]);

        // GREEN
        aadfPaletteColors[1][nNReadPaletteColors] = CPLAtof(papszTokens[2]);

        // BLUE
        aadfPaletteColors[2][nNReadPaletteColors] = CPLAtof(papszTokens[3]);

        // ALPHA
        aadfPaletteColors[3][nNReadPaletteColors] = 255;

        CSLDestroy(papszTokens);
        nNReadPaletteColors++;
    }

    if (nNReadPaletteColors != nNPaletteColors)
    {
        VSIFCloseL(fpColorTable);
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid color table: \"%s\"",
                 osColorTableFileName.c_str());
        return CE_Failure;
    }

    VSIFCloseL(fpColorTable);

    return CE_None;
}

/************************************************************************/
/*                               GetPCT()                               */
/*                                                                      */
/*      Return PCT information, if any exists.                          */
/************************************************************************/
void MMRBand::AssignRGBColor(int nIndexDstPalete, int nIndexSrcPalete)
{
    aadfPCT[0][nIndexDstPalete] = aadfPaletteColors[0][nIndexSrcPalete];
    aadfPCT[1][nIndexDstPalete] = aadfPaletteColors[1][nIndexSrcPalete];
    aadfPCT[2][nIndexDstPalete] = aadfPaletteColors[2][nIndexSrcPalete];
    aadfPCT[3][nIndexDstPalete] = aadfPaletteColors[3][nIndexSrcPalete];
}

void MMRBand::AssignRGBColorDirectly(int nIndexDstPalete, double dfValue)
{
    aadfPCT[0][nIndexDstPalete] = dfValue;
    aadfPCT[1][nIndexDstPalete] = dfValue;
    aadfPCT[2][nIndexDstPalete] = dfValue;
    aadfPCT[3][nIndexDstPalete] = dfValue;
}

// Converts palleteColors to Colors of pixels
CPLErr MMRBand::ConvertPaletteColors()

{
    //·$·TODO!!
    // Color_TractamentVariable=QuantitatiuContinu
    //Color_Paleta=PaletaOmbrejat.dbf
    //Color_EscalatColor=AssigDirecta
    //Color_ValorColor_0=0
    //Color_ValorColor_n_1=255

    // Categorical  %NODATA%
    // Alguns tenen mes de 255 categories (colors).
    //

    // ·$·TODO
    // Only for 1 or 2 bytes images
    if (eMMBytesPerPixel != MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE &&
        eMMBytesPerPixel != MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE)
    {
        return CE_None;
    }

    // Some necessary information
    if (!bMinSet || !bMaxSet)
        return CE_None;

    nNoDataPaletteIndex = 0;
    nNoDataOriginalIndex = 0;

    int nNPossibleValues =
        static_cast<int>(pow(2, static_cast<double>(8) *
                                    static_cast<int>(eMMBytesPerPixel))) *
        3L;
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPCT[iColumn].resize(nNPossibleValues);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    // Getting nNoDataPaletteIndex
    if (bNoDataSet)
    {
        if (eMMDataType == MMDataType::DATATYPE_AND_COMPR_INTEGER ||
            eMMDataType == MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE)
            nNoDataPaletteIndex = static_cast<int>(dfNoData) + 32768L;
        else
            nNoDataPaletteIndex = static_cast<int>(dfNoData);
    }

    // Number of real colors (appart from NoData)
    int nNPaletteColors = static_cast<int>(aadfPaletteColors[0].size());
    if (bPaletteHasNodata)
        nNPaletteColors--;

    if (static_cast<int>(eMMBytesPerPixel) > 2 &&
        nNPaletteColors < nNPossibleValues)
        return CE_None;

    int nFirstValidPaletteIndex;
    unsigned short nIndexColor;
    double dfSlope = 1, dfIntercept = 0;

    if (bPaletteHasNodata && nNoDataPaletteIndex == 0)
        nFirstValidPaletteIndex = 1;
    else
        nFirstValidPaletteIndex = 0;

    if (static_cast<int>(eMMBytesPerPixel) == 2)
    {
        // A scaling is applied between the minimum and maximum display values.
        dfSlope = nNPaletteColors / ((dfMax + 1 - dfMin));

        if (nNoDataPaletteIndex != 0)  // nodata at the end of the list
            dfIntercept = -dfSlope * dfMin;
        else
            dfIntercept = -dfSlope * dfMin + 1;
    }

    for (int nIPaletteColor = 0; nIPaletteColor < nNPossibleValues / 3;
         nIPaletteColor++)
    {
        if (bNoDataSet && nIPaletteColor == nNoDataPaletteIndex)
        {
            if (bPaletteHasNodata)
                AssignRGBColor(nIPaletteColor, nNoDataPaletteIndex);
            else
                AssignRGBColorDirectly(nIPaletteColor, 255);
        }
        else
        {
            if (nIPaletteColor < static_cast<int>(dfMin))
            {
                // Before the minimum, we apply the color of the first
                // element (as a placeholder).
                AssignRGBColor(nIPaletteColor, 0);
            }
            else if (nIPaletteColor <= static_cast<int>(dfMax))
            {
                // Between the minimum and maximum, we apply the value
                // read from the table.
                if (static_cast<int>(eMMBytesPerPixel) < 2)
                {
                    // The value is applied directly.
                    AssignRGBColor(nIPaletteColor, nFirstValidPaletteIndex);
                    nFirstValidPaletteIndex++;
                }
                else
                {
                    // The value is applied according to the scaling.
                    nIndexColor = static_cast<unsigned short>(
                        dfSlope * nIPaletteColor + dfIntercept);
                    AssignRGBColor(nIPaletteColor, nIndexColor);
                }
            }
            else
            {
                // After the maximum, we apply the value of the last
                // element(as a placeholder).
                AssignRGBColor(nIPaletteColor, nNPaletteColors - 1);
            }
        }
    }

    return CE_None;
}

CPLErr MMRBand::GetPCT()

{
    // If we haven't already tried to load the colors, do so now.
    if (aadfPCT[0].size() > 0)
        return CE_None;

    CPLString os_Color_Paleta_DBF = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Paleta");

    if (os_Color_Paleta_DBF.empty() || os_Color_Paleta_DBF == "<Automatic>")
        return CE_None;  // No color table available

    CPLString osExtension = CPLGetExtensionSafe(os_Color_Paleta_DBF);
    if (osExtension.tolower() == "dbf")
    {
        CPLErr peErr = GetPaletteColors_DBF(os_Color_Paleta_DBF);
        if (CE_None != peErr)
            return peErr;
    }
    else if (osExtension.tolower() == "pal" || osExtension.tolower() == "p25" ||
             osExtension.tolower() == "p65")
    {
        CPLErr peErr = GetPaletteColors_PAL_P25_P65(os_Color_Paleta_DBF);
        if (CE_None != peErr)
            return peErr;
    }
    else
        return CE_None;

    CPLErr peErr = ConvertPaletteColors();
    if (peErr != CE_None)
        return peErr;

    return CE_None;
}
