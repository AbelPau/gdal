/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRRasterband class.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//#include "cpl_port.h"
#include "miramon_dataset.h"
#include "miramon_rasterband.h"
#include "miramon_band.h"  // Per a MMRBand

#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()

/************************************************************************/
/*                           MMRRasterBand()                            */
/************************************************************************/
MMRRasterBand::MMRRasterBand(MMRDataset *poDSIn, int nBandIn)
    : pfRel(poDSIn->GetRel())
{
    poDS = poDSIn;
    nBand = nBandIn;

    eAccess = poDSIn->GetAccess();

    if (nBand < 0 || nBand > pfRel->GetNBands())
    {
        CPLAssert(false);
        return;
    }

    MMRBand *poBand = pfRel->GetBand(nBand - 1);

    // Getting some band info
    osBandSection = poBand->GetBandSection();
    eMMRDataTypeMiraMon = poBand->GeteMMDataType();
    eMMBytesPerPixel = poBand->GeteMMBytesPerPixel();
    nBlockXSize = poBand->GetBlockXSize();
    nBlockYSize = poBand->GetBlockYSize();

    SetDataType();

    // We have a valid RasterBand.
    SetIsValid(true);
}

/************************************************************************/
/*                           ~MMRRasterBand()                           */
/************************************************************************/

MMRRasterBand::~MMRRasterBand()

{
    FlushCache(true);

    if (poCT != nullptr)
        delete poCT;

    if (poDefaultRAT)
        delete poDefaultRAT;
}

/************************************************************************/
/*                             SetDataType()                         */
/************************************************************************/
void MMRRasterBand::SetDataType()
{
    switch (eMMRDataTypeMiraMon)
    {
        case MMDataType::DATATYPE_AND_COMPR_BIT:
        case MMDataType::DATATYPE_AND_COMPR_BYTE:
        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
            eDataType = GDT_Byte;
            break;

        case MMDataType::DATATYPE_AND_COMPR_UINTEGER:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
            eDataType = GDT_UInt16;
            break;

        case MMDataType::DATATYPE_AND_COMPR_INTEGER:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_ASCII:
            eDataType = GDT_Int16;
            break;

        case MMDataType::DATATYPE_AND_COMPR_LONG:
        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
            eDataType = GDT_Int32;
            break;

        case MMDataType::DATATYPE_AND_COMPR_REAL:
        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
        case MMDataType::DATATYPE_AND_COMPR_REAL_ASCII:
            eDataType = GDT_Float32;
            break;

        case MMDataType::DATATYPE_AND_COMPR_DOUBLE:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:
            eDataType = GDT_Float64;
            break;

        default:
            eDataType = GDT_Byte;
            // This should really report an error, but this isn't
            // so easy from within constructors.
            CPLDebug("GDAL", "Unsupported pixel type in MMRRasterBand: %d.",
                     static_cast<int>(eMMRDataTypeMiraMon));
            break;
    }
}

/************************************************************************/
/*                             GetNoDataValue()                         */
/************************************************************************/

double MMRRasterBand::GetNoDataValue(int *pbSuccess)

{
    double dfNoData = 0.0;

    if (nBand < 0 || (nBand - 1) > pfRel->GetNBands())
    {
        CPLAssert(false);
        return dfNoData;
    }

    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand)
    {
        CPLAssert(false);
        return dfNoData;
    }

    if (!poBand->BandHasNoData())
    {
        if (pbSuccess)
            *pbSuccess = FALSE;
        return dfNoData;
    }

    if (pbSuccess)
        *pbSuccess = TRUE;
    return poBand->GetNoDataValue();
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double MMRRasterBand::GetMinimum(int *pbSuccess)

{
    const char *pszValue = GetMetadataItem("STATISTICS_MINIMUM");

    if (pszValue != nullptr)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;
        return CPLAtofM(pszValue);
    }

    return GDALRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double MMRRasterBand::GetMaximum(int *pbSuccess)

{
    const char *pszValue = GetMetadataItem("STATISTICS_MAXIMUM");

    if (pszValue != nullptr)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;
        return CPLAtofM(pszValue);
    }

    return GDALRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MMRRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    CPLErr eErr = CE_None;

    if (nBand < 1 || nBand > pfRel->GetNBands())
        return CE_Failure;

    eErr = pfRel->GetBand(nBand - 1)->GetRasterBlock(
        nBlockXOff, nBlockYOff, pImage,
        nBlockXSize * nBlockYSize * GDALGetDataTypeSizeBytes(eDataType));

    if (eErr == CE_None &&
        eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BIT)
    {
        GByte *pabyData = static_cast<GByte *>(pImage);

        for (int nIAcumulated = nBlockXSize * nBlockYSize - 1;
             nIAcumulated >= 0; nIAcumulated--)
        {
            if ((pabyData[nIAcumulated >> 3] & (1 << (nIAcumulated & 0x7))))
                pabyData[nIAcumulated] = 1;
            else
                pabyData[nIAcumulated] = 0;
        }
    }

    return eErr;
}

/************************************************************************/
/*                         GetDescription()                             */
/************************************************************************/

const char *MMRRasterBand::GetDescription() const
{
    if (nBand < 1 || nBand > pfRel->GetNBands())
        return "";

    return pfRel->GetBand(nBand - 1)->GetBandName();
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp MMRRasterBand::GetColorInterpretation()

{
    if (poCT != nullptr)
        return GCI_PaletteIndex;

    return GCI_Undefined;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *MMRRasterBand::GetColorTable()
{
    if (poCT != nullptr || bTriedLoadColorTable)
        return poCT;

    poCT = new GDALColorTable();

    /*
    * GDALPaletteInterp
    */
    bTriedLoadColorTable = true;

    if (CE_None != UpdateTableColorsFromPalette())
    {
        // No color table available. Perhaps some attribute table with the colors?
        delete poCT;
        poCT = nullptr;
        return poCT;
    }

    ConvertColorsFromPaletteToColorTable();

    return poCT;
}

void MMRRasterBand::ConvertColorsFromPaletteToColorTable()
{
    int nColors = static_cast<int>(GetPCT_Red().size());

    if (nColors > 0)
    {
        for (int iColor = 0; iColor < nColors; iColor++)
        {
            GDALColorEntry sEntry = {
                static_cast<short int>(GetPCT_Red()[iColor]),
                static_cast<short int>(GetPCT_Green()[iColor]),
                static_cast<short int>(GetPCT_Blue()[iColor]),
                static_cast<short int>(GetPCT_Alpha()[iColor])};

            if ((sEntry.c1 < 0 || sEntry.c1 > 255) ||
                (sEntry.c2 < 0 || sEntry.c2 > 255) ||
                (sEntry.c3 < 0 || sEntry.c3 > 255) ||
                (sEntry.c4 < 0 || sEntry.c4 > 255))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Color table entry appears to be corrupt, skipping "
                         "the rest. ");
                nBlockXSize = 0;
                nBlockYSize = 0;
                break;
            }

            poCT->SetColorEntry(iColor, &sEntry);
        }
    }
}

CPLErr MMRRasterBand::UpdateTableColorsFromPalette()

{
    CPLString os_Color_TractamentVariable = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_TractamentVariable");

    CPLString os_Color_Const = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Const");

    if (os_Color_Const == "1")
        return AssignUniformColorTable();

    if (EQUAL(os_Color_TractamentVariable, "Categoric"))
        bColorTableCategorical = true;
    else
        bColorTableCategorical = false;

    CPLString os_Color_Paleta = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Paleta");

    if (os_Color_Paleta.empty() || os_Color_Paleta == "<Automatic>")
        return CE_Failure;

    CPLErr peErr = ReadPalette(os_Color_Paleta);
    if (CE_None != peErr)
        return peErr;

    if (bColorTableCategorical)
        peErr = FromPaletteToColorTableCategoricalMode();
    else
        peErr = FromPaletteToColorTableContinousMode();

    if (peErr != CE_None)
        return peErr;

    return CE_None;
}

CPLErr MMRRasterBand::UpdateAttributeColorsFromPalette()

{
    CPLString os_Color_TractamentVariable = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_TractamentVariable");

    CPLString os_Color_Const = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Const");

    if (os_Color_Const == "1")
        return AssignUniformColorTable();

    if (EQUAL(os_Color_TractamentVariable, "Categoric"))
        bColorTableCategorical = true;
    else
        bColorTableCategorical = false;

    CPLString os_Color_Paleta = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Paleta");

    if (os_Color_Paleta.empty() || os_Color_Paleta == "<Automatic>")
        return CE_Failure;

    CPLErr peErr = ReadPalette(os_Color_Paleta);
    if (CE_None != peErr)
        return peErr;

    if (bColorTableCategorical)
        peErr = FromPaletteToAttributeTableCategoricalMode();
    else
        peErr = FromPaletteToAttributeTableContinousMode();

    if (peErr != CE_None)
        return peErr;

    return CE_None;
}

CPLErr MMRRasterBand::AssignUniformColorTable()

{
    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    bConstantColor = true;

    // Example: Color_Smb=(255,0,255)
    CPLString os_Color_Smb =
        pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osBandSection, "Color_Smb");
    os_Color_Smb.replaceAll(" ", "");
    if (!os_Color_Smb.empty() && os_Color_Smb.size() >= 7 &&
        os_Color_Smb[0] == '(' && os_Color_Smb[os_Color_Smb.size() - 1] == ')')
    {
        os_Color_Smb.replaceAll("(", "");
        os_Color_Smb.replaceAll(")", "");
        char **papszTokens = CSLTokenizeString2(os_Color_Smb, ",", 0);
        if (CSLCount(papszTokens) != 3)
        {
            CSLDestroy(papszTokens);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"", pfRel->GetRELNameChar());
            return CE_Failure;
        }
        sConstantColorRGB.c1 = static_cast<short>(atoi(papszTokens[0]));
        sConstantColorRGB.c2 = static_cast<short>(atoi(papszTokens[1]));
        sConstantColorRGB.c3 = static_cast<short>(atoi(papszTokens[2]));
        sConstantColorRGB.c4 = 255;
        CSLDestroy(papszTokens);
    }

    // Only for 1 or 2 bytes images
    if (eMMBytesPerPixel != MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE &&
        eMMBytesPerPixel != MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE)
    {
        return CE_None;
    }

    int nNPossibleValues = static_cast<int>(
        pow(2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
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

    for (int nITableColor = 0; nITableColor < nNPossibleValues; nITableColor++)
    {
        if (poBand->BandHasNoData() && nITableColor == poBand->GetNoDataValue())
        {
            aadfPCT[0][nITableColor] = 0;
            aadfPCT[1][nITableColor] = 0;
            aadfPCT[2][nITableColor] = 0;
            aadfPCT[3][nITableColor] = 0;
        }
        else
        {
            // Before the minimum, we apply the color of the first
            // element (as a placeholder).
            aadfPCT[0][nITableColor] = sConstantColorRGB.c1;
            aadfPCT[1][nITableColor] = sConstantColorRGB.c2;
            aadfPCT[2][nITableColor] = sConstantColorRGB.c3;
            aadfPCT[3][nITableColor] = 255;
        }
    }

    return CE_None;
}

CPLErr MMRRasterBand::ReadPalette(CPLString os_Color_Paleta)
{
    if (bPaletteColorsRead)
        return CE_None;

    bPaletteColorsRead = true;

    CPLString osExtension = CPLGetExtensionSafe(os_Color_Paleta);
    if (osExtension.tolower() == "dbf")
    {
        CPLErr peErr;
        peErr = GetPaletteColors_DBF(os_Color_Paleta);

        if (CE_None != peErr)
            return peErr;
        return CE_None;
    }
    if (osExtension.tolower() == "pal" || osExtension.tolower() == "p25" ||
        osExtension.tolower() == "p65")
    {
        CPLErr peErr = GetPaletteColors_PAL_P25_P65(os_Color_Paleta);
        if (CE_None != peErr)
            return peErr;
        return CE_None;
    }

    return CE_Failure;  // A wrong given name
}

CPLErr MMRRasterBand::GetPaletteColors_DBF_Indexs(
    struct MM_DATA_BASE_XP &oColorTable, MM_EXT_DBF_N_FIELDS &nClauSimbol,
    MM_EXT_DBF_N_FIELDS &nRIndex, MM_EXT_DBF_N_FIELDS &nGIndex,
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
        return CE_Failure;

    return CE_None;
}

CPLErr MMRRasterBand::GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF)

{
    // Getting the full path name of the DBF
    CPLString osAux =
        CPLGetPathSafe(static_cast<const char *>(pfRel->GetRELNameChar()));
    CPLString osColorTableFileName =
        CPLFormFilenameSafe(osAux.c_str(), os_Color_Paleta_DBF.c_str(), "");

    // Reading the DBF file
    struct MM_DATA_BASE_XP oColorTable;
    memset(&oColorTable, 0, sizeof(oColorTable));

    if (MM_ReadExtendedDBFHeaderFromFile(
            osColorTableFileName.c_str(), &oColorTable,
            static_cast<const char *>(pfRel->GetRELNameChar())))
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Invalid color table:"
                 "\"%s\".",
                 osColorTableFileName.c_str());

        return CE_Failure;
    }

    // Getting indices of fiels that determine the colors.
    MM_EXT_DBF_N_FIELDS nClauSimbol, nRIndex, nGIndex, nBIndex;
    if (CE_Failure == GetPaletteColors_DBF_Indexs(oColorTable, nClauSimbol,
                                                  nRIndex, nGIndex, nBIndex))
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Invalid color table:"
                 "\"%s\".",
                 osColorTableFileName.c_str());

        VSIFCloseL(oColorTable.pfDataBase);
        MM_ReleaseMainFields(&oColorTable);
        return CE_Failure;
    }

    // Cheking the structure to be correct
    if (oColorTable.pField[nClauSimbol].BytesPerField == 0 ||
        oColorTable.pField[nRIndex].BytesPerField == 0 ||
        oColorTable.pField[nGIndex].BytesPerField == 0 ||
        oColorTable.pField[nBIndex].BytesPerField == 0 ||
        oColorTable.pField[nClauSimbol].FieldType != 'N' ||
        oColorTable.pField[nRIndex].FieldType != 'N' ||
        oColorTable.pField[nGIndex].FieldType != 'N' ||
        oColorTable.pField[nBIndex].FieldType != 'N')
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Invalid color table:"
                 "\"%s\".",
                 osColorTableFileName.c_str());

        VSIFCloseL(oColorTable.pfDataBase);
        MM_ReleaseMainFields(&oColorTable);
        return CE_Failure;
    }

    // Guessing or reading the number of colors of the palette.
    int nPCTColors;
    MM_EXT_DBF_N_RECORDS nIRecord;
    MM_ACCUMULATED_BYTES_TYPE_DBF nBufferSize = oColorTable.BytesPerRecord + 1;
    char *pzsRecord = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));
    char *pzsField = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));

    if (bColorTableCategorical)
    {
        // In categorical mode, the maximum CLAUSIMBOL value is the last color to read.

        nPCTColors = 0;

        VSIFSeekL(oColorTable.pfDataBase, oColorTable.FirstRecordOffset,
                  SEEK_SET);
        for (nIRecord = 0; nIRecord < oColorTable.nRecords; nIRecord++)
        {
            if (oColorTable.BytesPerRecord !=
                VSIFReadL(pzsRecord, sizeof(unsigned char),
                          oColorTable.BytesPerRecord, oColorTable.pfDataBase))
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "Invalid color table:"
                         "\"%s\".",
                         osColorTableFileName.c_str());
                return CE_Failure;
            }

            memcpy(pzsField,
                   pzsRecord + oColorTable.pField[nClauSimbol].AccumulatedBytes,
                   oColorTable.pField[nClauSimbol].BytesPerField);
            pzsField[oColorTable.pField[nClauSimbol].BytesPerField] = '\0';
            CPLString osField = pzsField;
            osField.replaceAll(" ", "");
            if (osField.empty())
                bPaletteHasNodata = true;
            else if (nPCTColors < atoi(osField))
                nPCTColors = atoi(osField);
        }
        nPCTColors++;  // Number is one more than the maximum

        // If there is nodata color, it has to be computed
        if (bPaletteHasNodata)
        {
            nNoDataPaletteIndex = nPCTColors;
            nPCTColors++;
        }
    }
    else
    {
        // In continous mode the number of the records is the number of te colors.
        nPCTColors = static_cast<int>(oColorTable.nRecords);  // Safe cast
    }

    // Checking the size of the palette.
    if (nPCTColors < 0 || nPCTColors > 65536)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid number of colors: %d "
                 "in color table \"%s\".",
                 nPCTColors, osColorTableFileName.c_str());

        VSIFCloseL(oColorTable.pfDataBase);
        MM_ReleaseMainFields(&oColorTable);
        return CE_Failure;
    }

    // Getting the memory to allocate the color values
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

    VSIFSeekL(oColorTable.pfDataBase, oColorTable.FirstRecordOffset, SEEK_SET);
    if (bColorTableCategorical)
    {
        // Each record's CLAUSIMBOL field matches a pixel value present in the raster,
        // enabling a direct mapping between raster values and color entries.
        //
        // Inicialization to black (semi-transparent)
        int nIPCTColors;
        for (nIPCTColors = 0; nIPCTColors < nPCTColors; nIPCTColors++)
        {
            aadfPaletteColors[0][nIPCTColors] = sDefaultColorRGB.c1;
            aadfPaletteColors[1][nIPCTColors] = sDefaultColorRGB.c2;
            aadfPaletteColors[2][nIPCTColors] = sDefaultColorRGB.c3;
            aadfPaletteColors[3][nIPCTColors] = sDefaultColorRGB.c4;
        }

        // Getting the indexs of the colors and assigning to aadfPaletteColors
        int nIPaletteIndex;
        for (nIRecord = 0; nIRecord < oColorTable.nRecords; nIRecord++)
        {
            if (oColorTable.BytesPerRecord !=
                VSIFReadL(pzsRecord, sizeof(unsigned char),
                          oColorTable.BytesPerRecord, oColorTable.pfDataBase))
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "Invalid color table:"
                         "\"%s\".",
                         osColorTableFileName.c_str());
                VSIFCloseL(oColorTable.pfDataBase);
                MM_ReleaseMainFields(&oColorTable);
                return CE_Failure;
            }

            // Nodata identification
            memcpy(pzsField,
                   pzsRecord + oColorTable.pField[nClauSimbol].AccumulatedBytes,
                   oColorTable.pField[nClauSimbol].BytesPerField);
            pzsField[oColorTable.pField[nClauSimbol].BytesPerField] = '\0';
            CPLString osField = pzsField;
            osField.replaceAll(" ", "");

            nIPaletteIndex = atoi(osField);

            AssignColorFromDBF(oColorTable, pzsRecord, pzsField, nRIndex,
                               nGIndex, nBIndex, nIPaletteIndex);
        }
    }
    else
    {
        /*
            Each record's CLAUSIMBOL field doesn't match a pixel value present in the raster,
            and it's used only for discovering nodata value (blanc value).
            The list of values is used to map every value in a color using:
                - Direct assignation: mode used in categorical modes but possible in continous.
                - Linear scaling
                - Logarithmic scaling
         */
        for (int nIPCTColors = 0; nIPCTColors < nPCTColors; nIPCTColors++)
        {
            if (oColorTable.BytesPerRecord !=
                VSIFReadL(pzsRecord, sizeof(unsigned char),
                          oColorTable.BytesPerRecord, oColorTable.pfDataBase))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid color table: \"%s\"",
                         osColorTableFileName.c_str());
                return CE_Failure;
            }

            // Nodata identification
            memcpy(pzsField,
                   pzsRecord + oColorTable.pField[nClauSimbol].AccumulatedBytes,
                   oColorTable.pField[nClauSimbol].BytesPerField);
            pzsField[oColorTable.pField[nClauSimbol].BytesPerField] = '\0';
            CPLString osField = pzsField;
            osField.replaceAll(" ", "");
            if (osField.empty())  // Nodata value
            {
                bPaletteHasNodata = true;
                nNoDataPaletteIndex = nIPCTColors;
            }

            AssignColorFromDBF(oColorTable, pzsRecord, pzsField, nRIndex,
                               nGIndex, nBIndex, nIPCTColors);
        }
    }

    VSIFree(pzsField);
    VSIFree(pzsRecord);
    VSIFCloseL(oColorTable.pfDataBase);
    MM_ReleaseMainFields(&oColorTable);

    return CE_None;
}

void MMRRasterBand::AssignColorFromDBF(struct MM_DATA_BASE_XP &oColorTable,
                                       char *pzsRecord, char *pzsField,
                                       MM_EXT_DBF_N_FIELDS &nRIndex,
                                       MM_EXT_DBF_N_FIELDS &nGIndex,
                                       MM_EXT_DBF_N_FIELDS &nBIndex,
                                       int nIPaletteIndex)
{
    // RED
    memcpy(pzsField, pzsRecord + oColorTable.pField[nRIndex].AccumulatedBytes,
           oColorTable.pField[nRIndex].BytesPerField);
    pzsField[oColorTable.pField[nRIndex].BytesPerField] = '\0';
    aadfPaletteColors[0][nIPaletteIndex] = CPLAtof(pzsField);

    // GREEN
    memcpy(pzsField, pzsRecord + oColorTable.pField[nGIndex].AccumulatedBytes,
           oColorTable.pField[nGIndex].BytesPerField);
    pzsField[oColorTable.pField[nGIndex].BytesPerField] = '\0';
    aadfPaletteColors[1][nIPaletteIndex] = CPLAtof(pzsField);

    // BLUE
    memcpy(pzsField, pzsRecord + oColorTable.pField[nBIndex].AccumulatedBytes,
           oColorTable.pField[nBIndex].BytesPerField);
    pzsField[oColorTable.pField[nBIndex].BytesPerField] = '\0';
    aadfPaletteColors[2][nIPaletteIndex] = CPLAtof(pzsField);

    // ALPHA
    if (aadfPaletteColors[0][nIPaletteIndex] == -1 &&
        aadfPaletteColors[1][nIPaletteIndex] == -1 &&
        aadfPaletteColors[2][nIPaletteIndex] == -1)
    {
        // (-1, -1, -1) is like nodata color
        bPaletteHasNodata = true;

        // Transparent (white or whatever color)
        aadfPaletteColors[0][nIPaletteIndex] = sNoDataColorRGB.c1;
        aadfPaletteColors[1][nIPaletteIndex] = sNoDataColorRGB.c2;
        aadfPaletteColors[2][nIPaletteIndex] = sNoDataColorRGB.c3;
        aadfPaletteColors[3][nIPaletteIndex] = sNoDataColorRGB.c4;
    }
    else
        aadfPaletteColors[3][nIPaletteIndex] = 255;
}

// Colors in a PAL, P25 or P65 format files
CPLErr
MMRRasterBand::GetPaletteColors_PAL_P25_P65(CPLString os_Color_Paleta_DBF)

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
        aadfPaletteColors[3][nNReadPaletteColors] = 255.0;

        CSLDestroy(papszTokens);
        nNReadPaletteColors++;
    }

    // Filling the rest of colors.
    for (int nIColorIndex = nNReadPaletteColors; nIColorIndex < nNPaletteColors;
         nIColorIndex++)
    {
        aadfPaletteColors[0][nNReadPaletteColors] = sDefaultColorRGB.c1;
        aadfPaletteColors[1][nNReadPaletteColors] = sDefaultColorRGB.c2;
        aadfPaletteColors[2][nNReadPaletteColors] = sDefaultColorRGB.c3;
        aadfPaletteColors[3][nNReadPaletteColors] = sDefaultColorRGB.c4;
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

void MMRRasterBand::AssignRGBColor(int nIndexDstCT, int nIndexSrcPalete)
{
    aadfPCT[0][nIndexDstCT] = aadfPaletteColors[0][nIndexSrcPalete];
    aadfPCT[1][nIndexDstCT] = aadfPaletteColors[1][nIndexSrcPalete];
    aadfPCT[2][nIndexDstCT] = aadfPaletteColors[2][nIndexSrcPalete];
    aadfPCT[3][nIndexDstCT] = aadfPaletteColors[3][nIndexSrcPalete];
}

void MMRRasterBand::AssignRGBColorDirectly(int nIndexDstCT, double dfValue)
{
    aadfPCT[0][nIndexDstCT] = dfValue;
    aadfPCT[1][nIndexDstCT] = dfValue;
    aadfPCT[2][nIndexDstCT] = dfValue;
    aadfPCT[3][nIndexDstCT] = dfValue;
}

// Converts pallete Colors to Colors of pixels
CPLErr MMRRasterBand::FromPaletteToColorTableCategoricalMode()

{
    // If the palette is not loaded, then, ignore the conversion silently
    if (aadfPaletteColors[0].size() == 0)
        return CE_Failure;

    int nNPossibleValues = static_cast<int>(
        pow(2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
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

    int nIPaletteColor = 0;
    // Giving color to the ones in the table
    int nNPaletteColors = static_cast<int>(aadfPaletteColors[0].size());

    // No more colors than needed.
    if (nNPaletteColors > nNPossibleValues)
        nNPaletteColors = nNPossibleValues;

    for (nIPaletteColor = 0; nIPaletteColor < nNPaletteColors; nIPaletteColor++)
    {
        aadfPCT[0][nIPaletteColor] = aadfPaletteColors[0][nIPaletteColor];
        aadfPCT[1][nIPaletteColor] = aadfPaletteColors[1][nIPaletteColor];
        aadfPCT[2][nIPaletteColor] = aadfPaletteColors[2][nIPaletteColor];
        aadfPCT[3][nIPaletteColor] = aadfPaletteColors[3][nIPaletteColor];
    }

    // Rest of colors
    for (; nIPaletteColor < nNPossibleValues; nIPaletteColor++)
    {
        aadfPCT[0][nIPaletteColor] = sDefaultColorRGB.c1;
        aadfPCT[1][nIPaletteColor] = sDefaultColorRGB.c2;
        aadfPCT[2][nIPaletteColor] = sDefaultColorRGB.c3;
        aadfPCT[3][nIPaletteColor] = sDefaultColorRGB.c4;
    }

    return CE_None;
}

// Converts palleteColors to Colors of pixels for the Color Table
CPLErr MMRRasterBand::FromPaletteToColorTableContinousMode()

{
    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    if (static_cast<int>(eMMBytesPerPixel) > 2)
        return CE_Failure;  // Attribute table

    // Some necessary information
    if (!poBand->GetMinVisuSet() || !poBand->GetMaxVisuSet())
        return CE_Failure;

    int nNPossibleValues = static_cast<int>(
        pow(2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
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

    // Number of real colors (appart from NoData)
    int nNPaletteColors = static_cast<int>(aadfPaletteColors[0].size());
    if (bPaletteHasNodata)
        nNPaletteColors--;

    // If palette doesn't have nodata last index is associated
    if (!bPaletteHasNodata)
        nNoDataPaletteIndex = nNPaletteColors;

    if (static_cast<int>(eMMBytesPerPixel) > 2 &&
        nNPaletteColors < nNPossibleValues)
        return CE_Failure;

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
        dfSlope = nNPaletteColors /
                  ((poBand->GetVisuMax() + 1 - poBand->GetVisuMin()));

        if (bPaletteHasNodata &&
            nNoDataPaletteIndex != 0)  // nodata at the end of the list
            dfIntercept = -dfSlope * poBand->GetVisuMin();
        else
            dfIntercept = -dfSlope * poBand->GetVisuMin() + 1;
    }

    for (int nIPaletteColor = 0; nIPaletteColor < nNPossibleValues;
         nIPaletteColor++)
    {
        if (poBand->BandHasNoData() && nIPaletteColor == nNoDataPaletteIndex)
        {
            if (bPaletteHasNodata)
                AssignRGBColor(nIPaletteColor, nNoDataPaletteIndex);
            else
                AssignRGBColorDirectly(nIPaletteColor, 255);
        }
        else
        {
            if (nIPaletteColor < static_cast<int>(poBand->GetVisuMin()))
            {
                // Before the minimum, we apply the color of the first
                // element (as a placeholder).
                AssignRGBColor(nIPaletteColor, 0);
            }
            else if (nIPaletteColor <= static_cast<int>(poBand->GetVisuMax()))
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

// Converts palleteColors to Colors of pixels in the attribute table
CPLErr MMRRasterBand::FromPaletteToAttributeTableContinousMode()

{
    MMRBand *poBand = pfRel->GetBand(nBand - 1);
    if (!poBand)
        return CE_Failure;

    // Some necessary information
    if (!poBand->GetMinVisuSet() || !poBand->GetMaxVisuSet())
        return CE_Failure;

    // Number of real colors (appart from NoData)
    int nNPaletteColors = static_cast<int>(aadfPaletteColors[0].size());
    int nRealNPaletteColors = nNPaletteColors;
    if (bPaletteHasNodata)
    {
        if (nNPaletteColors < 1)
            return CE_Failure;
        nNPaletteColors--;
    }

    if (nNPaletteColors <= 0)
        return CE_Failure;

    poDefaultRAT->CreateColumn("MIN", GFT_Real, GFU_Min);
    poDefaultRAT->CreateColumn("MAX", GFT_Real, GFU_Max);
    poDefaultRAT->CreateColumn("Red", GFT_Integer, GFU_Red);
    poDefaultRAT->CreateColumn("Green", GFT_Integer, GFU_Green);
    poDefaultRAT->CreateColumn("Blue", GFT_Integer, GFU_Blue);

    poDefaultRAT->SetTableType(GRTT_ATHEMATIC);
    poDefaultRAT->SetRowCount(
        static_cast<int>(nRealNPaletteColors));  // +1 for last element

    // If palette doesn't have nodata last index is associated
    if (!bPaletteHasNodata)
        nNoDataPaletteIndex = nNPaletteColors;

    // A scaling is applied between the minimum and maximum display values.
    double dfInterval =
        (poBand->GetVisuMax() - poBand->GetVisuMin()) / nNPaletteColors;

    if (poBand->BandHasNoData() && bPaletteHasNodata)
    {
        poDefaultRAT->SetValue(0, 0, poBand->GetNoDataValue());
        poDefaultRAT->SetValue(0, 1, poBand->GetNoDataValue());
        poDefaultRAT->SetValue(0, 2, aadfPaletteColors[0][nNoDataPaletteIndex]);
        poDefaultRAT->SetValue(0, 3, aadfPaletteColors[1][nNoDataPaletteIndex]);
        poDefaultRAT->SetValue(0, 4, aadfPaletteColors[2][nNoDataPaletteIndex]);
    }

    int nIPaletteColor = 0;
    for (; nIPaletteColor < nNPaletteColors; nIPaletteColor++)
    {
        poDefaultRAT->SetValue(nIPaletteColor + 1, 0,
                               poBand->GetVisuMin() +
                                   dfInterval * nIPaletteColor);
        poDefaultRAT->SetValue(
            nIPaletteColor + 1, 1,
            poBand->GetVisuMin() +
                dfInterval * (static_cast<double>(nIPaletteColor) + 1));
        poDefaultRAT->SetValue(nIPaletteColor + 1, 2,
                               aadfPaletteColors[0][nIPaletteColor]);
        poDefaultRAT->SetValue(nIPaletteColor + 1, 3,
                               aadfPaletteColors[1][nIPaletteColor]);
        poDefaultRAT->SetValue(nIPaletteColor + 1, 4,
                               aadfPaletteColors[2][nIPaletteColor]);
    }

    // Last interval
    poDefaultRAT->SetValue(nIPaletteColor, 0,
                           poBand->GetVisuMin() +
                               dfInterval *
                                   (static_cast<double>(nNPaletteColors) - 1));
    poDefaultRAT->SetValue(nIPaletteColor, 1, poBand->GetVisuMax());
    poDefaultRAT->SetValue(nIPaletteColor, 2,
                           aadfPaletteColors[0][nIPaletteColor - 1]);
    poDefaultRAT->SetValue(nIPaletteColor, 3,
                           aadfPaletteColors[1][nIPaletteColor - 1]);
    poDefaultRAT->SetValue(nIPaletteColor, 4,
                           aadfPaletteColors[2][nIPaletteColor - 1]);

    // Last value
    poDefaultRAT->SetValue(nIPaletteColor + 1, 0, poBand->GetVisuMax());
    poDefaultRAT->SetValue(nIPaletteColor + 1, 1, poBand->GetVisuMax());
    poDefaultRAT->SetValue(nIPaletteColor + 1, 2,
                           aadfPaletteColors[0][nIPaletteColor - 1]);
    poDefaultRAT->SetValue(nIPaletteColor + 1, 3,
                           aadfPaletteColors[1][nIPaletteColor - 1]);
    poDefaultRAT->SetValue(nIPaletteColor + 1, 4,
                           aadfPaletteColors[2][nIPaletteColor - 1]);

    return CE_None;
}

// Converts pallete Colors to Colors of pixels
CPLErr MMRRasterBand::FromPaletteToAttributeTableCategoricalMode()

{
    // TODO aixo va a la taula d'atributs en mode categòric
    // If the palette is not loaded, then, ignore the conversion silently
    if (aadfPaletteColors[0].size() == 0)
        return CE_Failure;

    int nNPossibleValues = static_cast<int>(
        pow(2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
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

    int nIPaletteColor = 0;
    // Giving color to the ones in the table
    int nNPaletteColors = static_cast<int>(aadfPaletteColors[0].size());

    // No more colors than needed.
    if (nNPaletteColors > nNPossibleValues)
        nNPaletteColors = nNPossibleValues;

    for (nIPaletteColor = 0; nIPaletteColor < nNPaletteColors; nIPaletteColor++)
    {
        aadfPCT[0][nIPaletteColor] = aadfPaletteColors[0][nIPaletteColor];
        aadfPCT[1][nIPaletteColor] = aadfPaletteColors[1][nIPaletteColor];
        aadfPCT[2][nIPaletteColor] = aadfPaletteColors[2][nIPaletteColor];
        aadfPCT[3][nIPaletteColor] = aadfPaletteColors[3][nIPaletteColor];
    }

    // Rest of colors
    for (; nIPaletteColor < nNPossibleValues; nIPaletteColor++)
    {
        aadfPCT[0][nIPaletteColor] = sDefaultColorRGB.c1;
        aadfPCT[1][nIPaletteColor] = sDefaultColorRGB.c2;
        aadfPCT[2][nIPaletteColor] = sDefaultColorRGB.c3;
        aadfPCT[3][nIPaletteColor] = sDefaultColorRGB.c4;
    }

    return CE_None;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr MMRRasterBand::SetMetadata(char **papszMDIn, const char *pszDomain)

{
    bMetadataDirty = true;

    return GDALPamRasterBand::SetMetadata(papszMDIn, pszDomain);
}

/************************************************************************/
/*                        SetMetadataItem()                             */
/************************************************************************/

CPLErr MMRRasterBand::SetMetadataItem(const char *pszTag, const char *pszValue,
                                      const char *pszDomain)

{
    bMetadataDirty = true;

    return GDALPamRasterBand::SetMetadataItem(pszTag, pszValue, pszDomain);
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *MMRRasterBand::GetDefaultRAT()

{
    if (poDefaultRAT != nullptr)
        return poDefaultRAT;

    poDefaultRAT = new GDALDefaultRasterAttributeTable();

    FillRATFromDBF();

    return poDefaultRAT;
}

/************************************************************************/
/*                           FillRATFromDBF()                           */
/************************************************************************/

CPLErr MMRRasterBand::FillRATFromDBF()

{
    CPLString os_IndexJoin = pfRel->GetMetadataValue(
        SECTION_ATTRIBUTE_DATA, osBandSection, "IndexsJoinTaula");

    if (os_IndexJoin.empty())
    {
        if (CE_None != UpdateAttributeColorsFromPalette())
        {
            delete poDefaultRAT;
            poDefaultRAT = nullptr;
            return CE_Failure;
        }

        //UUU
        //ConvertColorsFromPaletteToColorTable();

        return CE_None;
    }

    char **papszTokens = CSLTokenizeString2(os_IndexJoin, ",", 0);
    const int nTokens = CSLCount(papszTokens);

    if (nTokens < 1)
    {
        CSLDestroy(papszTokens);
        delete poDefaultRAT;
        poDefaultRAT = nullptr;
        return CE_Failure;
    }

    // TODO: només una de les taules
    for (int nIAttTable = 0; nIAttTable < nTokens; nIAttTable++)
    {
        CPLString osRELName, osDBFName, osAssociateRel;
        if (CE_None != GetAttributeTableName(papszTokens[nIAttTable], osRELName,
                                             osDBFName, osAssociateRel))
        {
            CSLDestroy(papszTokens);
            delete poDefaultRAT;
            poDefaultRAT = nullptr;
            return CE_Failure;
        }

        if (osDBFName.empty())
        {
            CSLDestroy(papszTokens);
            delete poDefaultRAT;
            poDefaultRAT = nullptr;
            return CE_Failure;
        }

        if (osAssociateRel.empty())
        {
            CSLDestroy(papszTokens);
            delete poDefaultRAT;
            poDefaultRAT = nullptr;
            return CE_Failure;
        }

        if (CE_None !=
            CreateAttributteTableFromDBF(osRELName, osDBFName, osAssociateRel))
        {
            CSLDestroy(papszTokens);
            delete poDefaultRAT;
            poDefaultRAT = nullptr;
            return CE_Failure;
        }
    }
    CSLDestroy(papszTokens);
    return CE_None;
}

CPLErr MMRRasterBand::CreateAttributteTableFromDBF(CPLString osRELName,
                                                   CPLString osDBFName,
                                                   CPLString osAssociateRel)
{
    struct MM_DATA_BASE_XP oAttributteTable;
    memset(&oAttributteTable, 0, sizeof(oAttributteTable));

    if (!osRELName.empty())
    {
        if (MM_ReadExtendedDBFHeaderFromFile(
                osDBFName.c_str(), &oAttributteTable,
                static_cast<const char *>(osRELName)))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error reading attribute table \"%s\".",
                     osDBFName.c_str());
            return CE_None;
        }
    }
    else
    {
        if (MM_ReadExtendedDBFHeaderFromFile(osDBFName.c_str(),
                                             &oAttributteTable, nullptr))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error reading attribute table \"%s\".",
                     osDBFName.c_str());
            return CE_None;
        }
    }

    MM_EXT_DBF_N_FIELDS nIField;
    MM_EXT_DBF_N_FIELDS nFieldIndex = oAttributteTable.nFields;
    MM_EXT_DBF_N_FIELDS nCategIndex = oAttributteTable.nFields;
    for (nIField = 0; nIField < oAttributteTable.nFields; nIField++)
    {
        if (EQUAL(oAttributteTable.pField[nIField].FieldName, osAssociateRel))
        {
            nFieldIndex = nIField;
            if (nIField + 1 < oAttributteTable.nFields)
                nCategIndex = nIField + 1;
            else if (nIField > 1)
                nCategIndex = nIField - 1;
            break;
        }
    }

    if (nFieldIndex == oAttributteTable.nFields)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid attribute table: \"%s\"",
                 oAttributteTable.szFileName);
        return CE_Failure;
    }

    if (oAttributteTable.pField[nFieldIndex].FieldType != 'N')
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid attribute table field: \"%s\"",
                 oAttributteTable.szFileName);
        return CE_Failure;
    }

    // 0 column: category value
    if (oAttributteTable.pField[nFieldIndex].DecimalsIfFloat)
    {
        if (CE_None != poDefaultRAT->CreateColumn(
                           oAttributteTable.pField[nFieldIndex].FieldName,
                           GFT_Real, GFU_MinMax))
            return CE_Failure;
    }
    else
    {
        if (CE_None != poDefaultRAT->CreateColumn(
                           oAttributteTable.pField[nFieldIndex].FieldName,
                           GFT_Integer, GFU_MinMax))
            return CE_Failure;
    }

    GDALRATFieldUsage eFieldUsage;
    GDALRATFieldType eFieldType;
    for (nIField = 0; nIField < oAttributteTable.nFields; nIField++)
    {
        if (nIField == nFieldIndex)
            continue;

        if (oAttributteTable.pField[nIField].FieldType == 'N')
        {
            eFieldUsage = GFU_MinMax;
            if (oAttributteTable.pField[nIField].DecimalsIfFloat)
                eFieldType = GFT_Real;
            else
                eFieldType = GFT_Integer;
        }
        else
        {
            eFieldUsage = GFU_Generic;
            eFieldType = GFT_String;
        }
        if (nIField == nCategIndex)
            eFieldUsage = GFU_Name;

        /*if(*oAttributteTable.pField[nIField].FieldDescription[0] != '\0')
        {
            if (CE_None != poDefaultRAT->CreateColumn(
                oAttributteTable.pField[nIField].FieldDescription[0],
                eFieldType, eFieldUsage))
                return CE_Failure;
        }
        else
        */
        {
            if (CE_None != poDefaultRAT->CreateColumn(
                               oAttributteTable.pField[nIField].FieldName,
                               eFieldType, eFieldUsage))
                return CE_Failure;
        }
    }

    VSIFSeekL(oAttributteTable.pfDataBase, oAttributteTable.FirstRecordOffset,
              SEEK_SET);
    poDefaultRAT->SetRowCount(static_cast<int>(oAttributteTable.nRecords));

    MM_ACCUMULATED_BYTES_TYPE_DBF nBufferSize =
        oAttributteTable.BytesPerRecord + 1;
    char *pzsRecord = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));
    char *pzsField = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));

    for (int nIRecord = 0;
         nIRecord < static_cast<int>(oAttributteTable.nRecords); nIRecord++)
    {
        if (oAttributteTable.BytesPerRecord !=
            VSIFReadL(pzsRecord, sizeof(unsigned char),
                      oAttributteTable.BytesPerRecord,
                      oAttributteTable.pfDataBase))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid attribute table: \"%s\"", osDBFName.c_str());
            return CE_Failure;
        }

        // Category index
        memcpy(pzsField,
               pzsRecord +
                   oAttributteTable.pField[nFieldIndex].AccumulatedBytes,
               oAttributteTable.pField[nFieldIndex].BytesPerField);
        pzsField[oAttributteTable.pField[nFieldIndex].BytesPerField] = '\0';
        CPLString osField = pzsField;
        poDefaultRAT->SetValue(nIRecord, 0, osField);

        int nIOrderedField = 1;
        for (nIField = 0; nIField < oAttributteTable.nFields; nIField++)
        {
            if (nIField == nFieldIndex)
                continue;

            // Category value
            memcpy(pzsField,
                   pzsRecord +
                       oAttributteTable.pField[nIField].AccumulatedBytes,
                   oAttributteTable.pField[nIField].BytesPerField);
            pzsField[oAttributteTable.pField[nIField].BytesPerField] = '\0';
            if (oAttributteTable.CharSet == MM_JOC_CARAC_OEM850_DBASE)
                MM_oemansi(pzsField);

            if (oAttributteTable.CharSet != MM_JOC_CARAC_UTF8_DBF)
            {
                // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode to UTF-8
                char *pszFieldRecoded =
                    CPLRecode(pzsField, CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                poDefaultRAT->SetValue(nIRecord, nIOrderedField,
                                       pszFieldRecoded);
                CPLFree(pszFieldRecoded);
            }
            else
                poDefaultRAT->SetValue(nIRecord, nIOrderedField, pzsField);

            nIOrderedField++;
        }
    }

    VSIFree(pzsField);
    VSIFree(pzsRecord);
    VSIFCloseL(oAttributteTable.pfDataBase);
    MM_ReleaseMainFields(&oAttributteTable);

    return CE_None;
}

CPLErr MMRRasterBand::GetAttributeTableName(char *papszToken,
                                            CPLString &osRELName,
                                            CPLString &osDBFName,
                                            CPLString &osAssociateREL)
{
    CPLString os_Join = "JoinTaula";
    os_Join.append("_");
    os_Join.append(papszToken);

    CPLString osTableNameSection_value =
        pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osBandSection, os_Join);

    if (osTableNameSection_value.empty())
        return CE_Failure;  // No attribute available

    CPLString osTableNameSection = "TAULA_";
    osTableNameSection.append(osTableNameSection_value);

    CPLString osShortRELName =
        pfRel->GetMetadataValue(osTableNameSection, "NomFitxer");

    CPLString osExtension = CPLGetExtensionSafe(osShortRELName);
    if (osExtension.tolower() == "rel")
    {
        // Get path relative to REL file
        osRELName =
            CPLFormFilenameSafe(CPLGetPathSafe(pfRel->GetRELNameChar()).c_str(),
                                osShortRELName, "");

        // Getting information from the associated REL
        MMRRel *fLocalRel = new MMRRel(osRELName);
        CPLString osShortDBFName =
            fLocalRel->GetMetadataValue("TAULA_PRINCIPAL", "NomFitxer");

        if (osShortDBFName.empty())
        {
            osRELName = "";
            delete fLocalRel;
            return CE_Failure;
        }

        // Get path relative to REL file
        osDBFName = CPLFormFilenameSafe(
            CPLGetPathSafe(fLocalRel->GetRELNameChar()).c_str(), osShortDBFName,
            "");

        osAssociateREL =
            fLocalRel->GetMetadataValue("TAULA_PRINCIPAL", "AssociatRel");

        if (osAssociateREL.empty())
        {
            osRELName = "";
            delete fLocalRel;
            return CE_Failure;
        }

        CPLString osSection = "TAULA_PRINCIPAL:";
        osSection.append(osAssociateREL);

        CPLString osTactVar =
            fLocalRel->GetMetadataValue(osSection, "TractamentVariable");
        if (osTactVar == "Categoric")
            poDefaultRAT->SetTableType(GRTT_THEMATIC);
        else
            poDefaultRAT->SetTableType(GRTT_ATHEMATIC);

        delete fLocalRel;
        return CE_None;
    }

    osExtension = CPLGetExtensionSafe(osShortRELName);
    if (osExtension.tolower() == "dbf")
    {
        // Get path relative to REL file
        osDBFName =
            CPLFormFilenameSafe(CPLGetPathSafe(pfRel->GetRELNameChar()).c_str(),
                                osShortRELName, "");

        osAssociateREL =
            pfRel->GetMetadataValue(osTableNameSection, "AssociatRel");

        return CE_None;
    }

    osRELName = "";
    osAssociateREL = "";

    return CE_Failure;
}
