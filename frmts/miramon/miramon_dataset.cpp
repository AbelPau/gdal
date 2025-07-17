/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRDataset class.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//#include "cpl_port.h"
#include "miramon_dataset.h"
#include "miramon_band.h"  // Per a MMRBand

#ifdef MSVC
#include "..\miramon_common\mm_gdal_functions.h"  // For MMCheck_REL_FILE()
#else
#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()
#endif

/************************************************************************/
/*                           MMRRasterBand()                            */
/************************************************************************/
MMRRasterBand::MMRRasterBand(MMRDataset *poDSIn, int nBandIn)
    : hMMR(poDSIn->GetMMRInfo())
{
    poDS = poDSIn;
    nBand = nBandIn;

    eAccess = poDSIn->GetAccess();

    if (nBand < 0 || nBand > hMMR->nBands)
    {
        CPLAssert(false);
        return;
    }

    MMRBand *poBand = hMMR->papoBand[nBand - 1];

    // Getting some band info
    osBandSection = poBand->GetBandSection();
    eMMRDataTypeMiraMon = poBand->GeteMMDataType();
    eMMBytesPerPixel = poBand->GeteMMBytesPerPixel();
    nBlockXSize = poBand->nBlockXSize;
    nBlockYSize = poBand->nBlockYSize;

    SetDataType();
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

    if (nBand < 0 || (nBand - 1) > hMMR->nBands)
    {
        CPLAssert(false);
        return dfNoData;
    }

    MMRBand *poBand = hMMR->papoBand[nBand - 1];
    if (!poBand)
    {
        CPLAssert(false);
        return dfNoData;
    }

    if (!poBand->bNoDataSet)
    {
        if (pbSuccess)
            *pbSuccess = FALSE;
        return dfNoData;
    }

    if (pbSuccess)
        *pbSuccess = TRUE;
    return poBand->dfNoData;
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

    if (nBand < 1 || nBand > hMMR->nBands)
        return CE_Failure;

    eErr = hMMR->papoBand[nBand - 1]->GetRasterBlock(
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
    if (nBand < 1 || nBand > hMMR->nBands)
        return "";

    return hMMR->papoBand[nBand - 1]->GetBandName();
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

    if (CE_None != UpdateCategoricalColors())
    {
        // No color table available
        delete poCT;
        poCT = nullptr;
    }

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

    return poCT;
}

CPLErr MMRRasterBand::UpdateContinousColors()

{
    CPLString os_Color_TractamentVariable = hMMR->fRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_TractamentVariable");

    if (EQUAL(os_Color_TractamentVariable, "Categoric"))
        return CE_Failure;  // Color table instead

    CPLString os_Color_Paleta = hMMR->fRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Paleta");

    if (os_Color_Paleta.empty() || os_Color_Paleta == "<Automatic>")
        return CE_Failure;

    CPLErr peErr = ReadPalette(os_Color_Paleta, false);
    if (CE_None != peErr)
        return peErr;

    peErr = FromPaletteToTableCategoricalMode();
    if (peErr != CE_None)
        return peErr;

    return CE_None;
}

CPLErr MMRRasterBand::UpdateCategoricalColors()

{
    CPLString os_Color_TractamentVariable = hMMR->fRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_TractamentVariable");

    CPLString os_Color_Const = hMMR->fRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Const");

    if (os_Color_Const == "1")
        return AssignUniformColorTable();

    if (!EQUAL(os_Color_TractamentVariable, "Categoric"))
        return CE_Failure;  // Attribute table instead

    CPLString os_Color_Paleta = hMMR->fRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Paleta");

    if (os_Color_Paleta.empty() || os_Color_Paleta == "<Automatic>")
        return CE_Failure;

    CPLErr peErr = ReadPalette(os_Color_Paleta, true);
    if (CE_None != peErr)
        return peErr;

    peErr = FromPaletteToTableCategoricalMode();
    if (peErr != CE_None)
        return peErr;

    return CE_None;
}

CPLErr MMRRasterBand::AssignUniformColorTable()

{
    MMRBand *poBand = hMMR->papoBand[nBand - 1];
    if (!poBand)
        return CE_Failure;

    bConstantColor = true;

    // Example: Color_Smb=(255,0,255)
    CPLString os_Color_Smb = hMMR->fRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Smb");
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
                     "Invalid constant color: \"%s\"",
                     hMMR->fRel->GetRELNameChar());
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
        if (poBand->bNoDataSet && nITableColor == poBand->dfNoData)
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

CPLErr MMRRasterBand::ReadPalette(CPLString os_Color_Paleta,
                                  bool bCategoricalMode)
{
    CPLString osExtension = CPLGetExtensionSafe(os_Color_Paleta);
    if (osExtension.tolower() == "dbf")
    {
        CPLErr peErr;
        peErr = GetPaletteColors_DBF(os_Color_Paleta, bCategoricalMode);

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

CPLErr MMRRasterBand::GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF,
                                           bool bCategoricalMode)

{
    // Getting the full path name of the DBF
    CPLString osAux =
        CPLGetPathSafe(static_cast<const char *>(hMMR->fRel->GetRELNameChar()));
    CPLString osColorTableFileName =
        CPLFormFilenameSafe(osAux.c_str(), os_Color_Paleta_DBF.c_str(), "");

    // Reading the DBF file
    struct MM_DATA_BASE_XP oColorTable;
    memset(&oColorTable, 0, sizeof(oColorTable));

    if (MM_ReadExtendedDBFHeaderFromFile(
            osColorTableFileName.c_str(), &oColorTable,
            static_cast<const char *>(hMMR->fRel->GetRELNameChar())))
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

    if (bCategoricalMode)
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
            nNoDataOriginaPalettelIndex = nPCTColors;
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

    if (bCategoricalMode)
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
                nNoDataOriginaPalettelIndex = nIPCTColors;
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
        CPLGetPathSafe(static_cast<const char *>(hMMR->fRel->GetRELNameChar()));
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

void MMRRasterBand::AssignRGBColor(int nIndexDstPalete, int nIndexSrcPalete)
{
    aadfPCT[0][nIndexDstPalete] = aadfPaletteColors[0][nIndexSrcPalete];
    aadfPCT[1][nIndexDstPalete] = aadfPaletteColors[1][nIndexSrcPalete];
    aadfPCT[2][nIndexDstPalete] = aadfPaletteColors[2][nIndexSrcPalete];
    aadfPCT[3][nIndexDstPalete] = aadfPaletteColors[3][nIndexSrcPalete];
}

void MMRRasterBand::AssignRGBColorDirectly(int nIndexDstPalete, double dfValue)
{
    aadfPCT[0][nIndexDstPalete] = dfValue;
    aadfPCT[1][nIndexDstPalete] = dfValue;
    aadfPCT[2][nIndexDstPalete] = dfValue;
    aadfPCT[3][nIndexDstPalete] = dfValue;
}

// Converts palleteColors to Colors of pixels
CPLErr MMRRasterBand::FromPaletteToTableCategoricalMode()

{
    // If the palette is not loaded, then, ignore the conversion silently
    if (aadfPaletteColors[0].size() == 0)
        return CE_None;

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

// Converts palleteColors to Colors of pixels
CPLErr MMRRasterBand::FromPaletteToTableContinousMode()

{
    MMRBand *poBand = hMMR->papoBand[nBand - 1];
    if (!poBand)
        return CE_Failure;

    // Only for 1 or 2 bytes images
    if (eMMBytesPerPixel != MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE &&
        eMMBytesPerPixel != MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE)
    {
        return CE_None;
    }

    // Some necessary information
    if (!poBand->GetMinVisuSet() || !poBand->GetMaxVisuSet())
        return CE_None;

    nNoDataPaletteIndex = 0;
    nNoDataOriginaPalettelIndex = 0;

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
    if (poBand->bNoDataSet)
    {
        if (poBand->GeteMMDataType() ==
                MMDataType::DATATYPE_AND_COMPR_INTEGER ||
            poBand->GeteMMDataType() ==
                MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE)
            nNoDataOriginaPalettelIndex =
                static_cast<int>(poBand->dfNoData) + 32768L;
        else
            nNoDataOriginaPalettelIndex = static_cast<int>(poBand->dfNoData);
    }

    // Number of real colors (appart from NoData)
    int nNPaletteColors = static_cast<int>(aadfPaletteColors[0].size());
    if (bPaletteHasNodata)
        nNPaletteColors--;

    // If palette don't has nodata last index is associated
    if (!bPaletteHasNodata)
        nNoDataPaletteIndex = nNPaletteColors;

    if (static_cast<int>(eMMBytesPerPixel) > 2 &&
        nNPaletteColors < nNPossibleValues)
        return CE_None;

    int nFirstValidPaletteIndex;
    unsigned short nIndexColor;
    double dfSlope = 1, dfIntercept = 0;

    if (bPaletteHasNodata && nNoDataOriginaPalettelIndex == 0)
        nFirstValidPaletteIndex = 1;
    else
        nFirstValidPaletteIndex = 0;

    if (static_cast<int>(eMMBytesPerPixel) == 2)
    {
        // A scaling is applied between the minimum and maximum display values.
        dfSlope = nNPaletteColors /
                  ((poBand->GetVisuMax() + 1 - poBand->GetVisuMin()));

        if (nNoDataPaletteIndex != 0)  // nodata at the end of the list
            dfIntercept = -dfSlope * poBand->GetVisuMin();
        else
            dfIntercept = -dfSlope * poBand->GetVisuMin() + 1;
    }

    for (int nIPaletteColor = 0; nIPaletteColor < nNPossibleValues / 3;
         nIPaletteColor++)
    {
        if (poBand->bNoDataSet && nIPaletteColor == nNoDataOriginaPalettelIndex)
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
    CPLString os_IndexJoin = hMMR->fRel->GetMetadataValue(
        SECTION_ATTRIBUTE_DATA, osBandSection, "IndexsJoinTaula");

    if (os_IndexJoin.empty())
    {
        // No attribute available
        delete poDefaultRAT;
        poDefaultRAT = nullptr;
        return CE_Failure;
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

    CPLString osTableNameSection_value = hMMR->fRel->GetMetadataValue(
        SECTION_ATTRIBUTE_DATA, osBandSection, os_Join);

    if (osTableNameSection_value.empty())
        return CE_Failure;  // No attribute available

    CPLString osTableNameSection = "TAULA_";
    osTableNameSection.append(osTableNameSection_value);

    CPLString osShortRELName =
        hMMR->fRel->GetMetadataValue(osTableNameSection, "NomFitxer");

    CPLString osExtension = CPLGetExtensionSafe(osShortRELName);
    if (osExtension.tolower() == "rel")
    {
        // Get path relative to REL file
        osRELName = CPLFormFilenameSafe(
            CPLGetPathSafe(hMMR->fRel->GetRELNameChar()).c_str(),
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
        osDBFName = CPLFormFilenameSafe(
            CPLGetPathSafe(hMMR->fRel->GetRELNameChar()).c_str(),
            osShortRELName, "");

        osAssociateREL =
            hMMR->fRel->GetMetadataValue(osTableNameSection, "AssociatRel");

        return CE_None;
    }

    osRELName = "";
    osAssociateREL = "";

    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                            MMRInfo                                   */
/* ==================================================================== */
/************************************************************************/

MMRInfo::MMRInfo(char *pszFilename)
{
    // Creates the object that allows inspect metadata (REL file)
    fRel = new MMRRel(pszFilename);

    // Sets the info from that REL
    if (CE_None != fRel->UpdateInfoFromREL(pszFilename, *this))
        return;
}

/************************************************************************/
/*                              ~MMRInfo()                              */
/************************************************************************/
MMRInfo::~MMRInfo()
{
    for (int i = 0; i < nBands; i++)
        delete papoBand[i];

    delete[] papoBand;

    delete fRel;
}

/************************************************************************/
/*                            MMRDataset()                            */
/************************************************************************/

MMRDataset::MMRDataset(GDALOpenInfo *poOpenInfo)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    // Creating the class MMRInfo.
    auto pMMInfo = std::make_unique<MMRInfo>(poOpenInfo->pszFilename);

    if (pMMInfo->nBands == 0)
    {
        if (pMMInfo->bIsAMiraMonFile)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to open %s, it has zero usable bands.",
                     poOpenInfo->pszFilename);
        }
        return;
    }

    hMMR = pMMInfo.release();

    // General Dataset information available
    nRasterXSize = hMMR->nXSize;
    nRasterYSize = hMMR->nYSize;
    GetDataSetBoundingBox();  // Fills adfGeoTransform
    ReadProjection();
    nBands = 0;

    // Assign every band to a subdataset (if any)
    // If all bands should go to a one single Subdataset, then,
    // no subdataset will be created and all bands will go to this
    // dataset.
    AssignBandsToSubdataSets();

    // Create subdatasets or add bands, as needed
    if (nNSubdataSets)
        CreateSubdatasetsFromBands();
    else
        AssignBands();
}

/************************************************************************/
/*                           ~MMRDataset()                            */
/************************************************************************/

MMRDataset::~MMRDataset()

{
    // Destroy the raster bands if they exist.  We forcibly clean
    // them up now to avoid any effort to write to them after the
    // file is closed.
    for (int i = 0; i < nBands && papoBands != nullptr; i++)
    {
        if (papoBands[i] != nullptr)
            delete papoBands[i];
    }
    CPLFree(papoBands);
    papoBands = nullptr;

    delete hMMR;
}

/************************************************************************/
/*                           ReadProjection()                           */
/************************************************************************/
CPLErr MMRDataset::ReadProjection()

{
    if (!hMMR->fRel)
        return CE_Failure;

    CPLString osSRS = hMMR->fRel->GetMetadataValue(
        "SPATIAL_REFERENCE_SYSTEM:HORIZONTAL", "HorizontalSystemIdentifier");

    char szResult[MM_MAX_ID_SNY + 10];
    int nResult = ReturnEPSGCodeSRSFromMMIDSRS(osSRS.c_str(), szResult);
    if (nResult == 1 || szResult[0] == '\0')
        return CE_Failure;

    m_oSRS.importFromEPSG(atoi(szResult));

    return m_oSRS.IsEmpty() ? CE_Failure : CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/
int MMRDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    // Checking for subdataset
    int nIdentifyResult =
        MMRRel::IdentifySubdataSetFile(poOpenInfo->pszFilename);
    if (nIdentifyResult != GDAL_IDENTIFY_FALSE)
        return nIdentifyResult;

    // Checking for MiraMon raster file
    return MMRRel::IdentifyFile(poOpenInfo);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *MMRDataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Verify that this is a MMR file.
    if (!Identify(poOpenInfo))
        return nullptr;

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The MiraMonRaster driver does not support update "
                 "access to existing datasets.");
        return nullptr;
    }

    // Create the Dataset (with bands or Subdatasets).
    auto poDS = std::make_unique<MMRDataset>(poOpenInfo);
    if (poDS->hMMR == nullptr)
        return nullptr;

    // Set description
    poDS->SetDescription(poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *MMRDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr MMRDataset::SetMetadata(char **papszMDIn, const char *pszDomain)

{
    bMetadataDirty = true;

    return GDALPamDataset::SetMetadata(papszMDIn, pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr MMRDataset::SetMetadataItem(const char *pszTag, const char *pszValue,
                                   const char *pszDomain)

{
    bMetadataDirty = true;

    return GDALPamDataset::SetMetadataItem(pszTag, pszValue, pszDomain);
}

/************************************************************************/
/*                          GetDataSetBoundingBox()                     */
/************************************************************************/
int MMRDataset::GetDataSetBoundingBox()
{
    // Bounding box of the band
    // Section [EXTENT] in rel file

    m_gt[0] = 0.0;
    m_gt[1] = 1.0;
    m_gt[2] = 0.0;
    m_gt[3] = 0.0;
    m_gt[4] = 0.0;
    m_gt[5] = 1.0;

    if (!hMMR || !hMMR->fRel)
        return 1;

    CPLString osMinX = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MinX");
    if (osMinX.empty())
        return 1;
    m_gt[0] = atof(osMinX);

    int nNCols = hMMR->fRel->GetColumnsNumberFromREL();
    if (nNCols <= 0)
        return 1;

    CPLString osMaxX = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MaxX");
    if (osMaxX.empty())
        return 1;

    m_gt[1] = (atof(osMaxX) - m_gt[0]) / nNCols;
    m_gt[2] = 0.0;  // No rotation in MiraMon rasters

    CPLString osMinY = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MinY");
    if (osMinY.empty())
        return 1;

    CPLString osMaxY = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MaxY");
    if (osMaxY.empty())
        return 1;

    int nNRows = hMMR->fRel->GetRowsNumberFromREL();
    if (nNRows <= 0)
        return 1;

    m_gt[3] = atof(osMaxY);
    m_gt[4] = 0.0;
    m_gt[5] = (atof(osMinY) - m_gt[3]) / nNRows;

    return 0;
}

int MMRDataset::GetBandBoundingBox(int nIBand)
{
    // Bounding box of the band
    m_gt[0] = 0.0;
    m_gt[1] = 1.0;
    m_gt[2] = 0.0;
    m_gt[3] = 0.0;
    m_gt[4] = 0.0;
    m_gt[5] = 1.0;

    if (!hMMR || !hMMR->papoBand || nIBand >= hMMR->nBands ||
        !hMMR->papoBand[nIBand])
        return 1;

    MMRBand *poBand = hMMR->papoBand[nIBand];

    m_gt[0] = poBand->GetBoundingBoxMinX();
    m_gt[1] = (poBand->GetBoundingBoxMaxX() - m_gt[0]) / poBand->nWidth;
    m_gt[2] = 0.0;  // No rotation in MiraMon rasters
    m_gt[3] = poBand->GetBoundingBoxMaxY();
    m_gt[4] = 0.0;
    m_gt[5] = (poBand->GetBoundingBoxMinY() - m_gt[3]) / poBand->nHeight;

    return 0;
}

CPLErr MMRDataset::GetGeoTransform(GDALGeoTransform &gt) const
{
    if (m_gt[0] != 0.0 || m_gt[1] != 1.0 || m_gt[2] != 0.0 || m_gt[3] != 0.0 ||
        m_gt[4] != 0.0 || m_gt[5] != 1.0)
    {
        gt = m_gt;
        return CE_None;
    }

    //return GDALPamDataset::GetGeoTransform(padfTransform);
    return GDALDataset::GetGeoTransform(gt);
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/
/*
int MMRDataset::GetGCPCount()
{
    const int nPAMCount = GDALPamDataset::GetGCPCount();
    return nPAMCount > 0 ? nPAMCount : static_cast<int>(m_aoGCPs.size());
}*/

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/
/*
const OGRSpatialReference *MMRDataset::GetGCPSpatialRef() const

{
    const OGRSpatialReference *poSRS = GDALPamDataset::GetGCPSpatialRef();
    if (poSRS)
        return poSRS;
    return !m_aoGCPs.empty() && !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
}*/

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/
/*
const GDAL_GCP *MMRDataset::GetGCPs()
{
    const GDAL_GCP *psPAMGCPs = GDALPamDataset::GetGCPs();
    if (psPAMGCPs)
        return psPAMGCPs;
    return gdal::GCP::c_ptr(m_aoGCPs);
}*/

/************************************************************************/
/*                GDALRegister_MiraMon()                                */
/************************************************************************/

void GDALRegister_MiraMon()

{
    if (GDALGetDriverByName("MiraMonRaster") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MiraMonRaster");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MiraMon Raster Images");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/miramon.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "rel");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "rel img");
    // For the writing part
    // poDriver->SetMetadataItem(
    //    GDAL_DMD_CREATIONDATATYPES,
    //    "Byte Int8 Int16 UInt16 Int32 UInt32 Float32 Float64 "
    //    "CFloat32 CFloat64");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnOpen = MMRDataset::Open;
    poDriver->pfnIdentify = MMRDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

bool MMRDataset::NextBandInANewDataSet(int nIBand)
{
    if (nIBand < 0)
        return false;

    if (nIBand + 1 >= hMMR->nBands)
        return false;

    MMRBand *pThisBand = hMMR->papoBand[nIBand];
    MMRBand *pNextBand = hMMR->papoBand[nIBand + 1];

    // Two images with different numbers of columns are assigned to different subdatasets
    if (pThisBand->nWidth != pNextBand->nWidth)
        return true;

    // Two images with different numbers of rows are assigned to different subdatasets
    if (pThisBand->nHeight != pNextBand->nHeight)
        return true;

    // Two images with different resolution are assigned to different subdatasets
    if (pThisBand->GetPixelResolution() != pNextBand->GetPixelResolution())
        return true;

    // Two images with different bounding box are assigned to different subdatasets
    if (pThisBand->GetBoundingBoxMinX() != pNextBand->GetBoundingBoxMinX())
        return true;
    if (pThisBand->GetBoundingBoxMaxX() != pNextBand->GetBoundingBoxMaxX())
        return true;
    if (pThisBand->GetBoundingBoxMinY() != pNextBand->GetBoundingBoxMinY())
        return true;
    if (pThisBand->GetBoundingBoxMaxY() != pNextBand->GetBoundingBoxMaxY())
        return true;

    // One image has NoData values and the other does not;
    // they are assigned to different subdatasets
    if (pThisBand->bNoDataSet != pNextBand->bNoDataSet)
        return true;

    // Two images with different NoData values are assigned to different subdatasets
    if (pThisBand->dfNoData != pNextBand->dfNoData)
        return true;

    return false;
}

// Assigns every band to a subdataset
void MMRDataset::AssignBandsToSubdataSets()
{
    nNSubdataSets = 0;
    if (!hMMR->papoBand)
        return;

    nNSubdataSets = 1;
    int nIBand = 0;
    hMMR->papoBand[nIBand]->AssignSubDataSet(nNSubdataSets);
    for (; nIBand < hMMR->nBands - 1; nIBand++)
    {
        if (NextBandInANewDataSet(nIBand))
        {
            nNSubdataSets++;
            hMMR->papoBand[nIBand + 1]->AssignSubDataSet(nNSubdataSets);
        }
        else
            hMMR->papoBand[nIBand + 1]->AssignSubDataSet(nNSubdataSets);
    }

    // If there is only one subdataset, it means that
    // we don't need subdatasets (all assigned to 0)
    if (nNSubdataSets == 1)
    {
        nNSubdataSets = 0;
        for (nIBand = 0; nIBand < hMMR->nBands; nIBand++)
            hMMR->papoBand[nIBand]->AssignSubDataSet(nNSubdataSets);
    }
}

void MMRDataset::CreateSubdatasetsFromBands()
{
    CPLStringList oSubdatasetList;
    CPLString osDSName;
    CPLString osDSDesc;

    for (int iSubdataset = 1; iSubdataset <= nNSubdataSets; iSubdataset++)
    {
        int nIBand;
        for (nIBand = 0; nIBand < hMMR->nBands; nIBand++)
        {
            if (hMMR->papoBand[nIBand]->GetAssignedSubDataSet() == iSubdataset)
                break;
        }

        // ·$·TODO passar els noms a una funció que determini si calen cometes.
        osDSName.Printf("MiraMonRaster:\"%s\",\"%s\"",
                        hMMR->papoBand[nIBand]->GetRELFileName().c_str(),
                        hMMR->papoBand[nIBand]->GetRawBandFileName().c_str());
        osDSDesc.Printf("Subdataset %d: \"%s\"", iSubdataset,
                        hMMR->papoBand[nIBand]->GetBandName().c_str());
        nIBand++;

        for (; nIBand < hMMR->nBands; nIBand++)
        {
            if (hMMR->papoBand[nIBand]->GetAssignedSubDataSet() != iSubdataset)
                continue;

            osDSName.append(CPLSPrintf(
                ",\"%s\"",
                hMMR->papoBand[nIBand]->GetRawBandFileName().c_str()));
            osDSDesc.append(CPLSPrintf(
                ",\"%s\"", hMMR->papoBand[nIBand]->GetBandName().c_str()));
        }

        oSubdatasetList.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_NAME", iSubdataset), osDSName);
        oSubdatasetList.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_DESC", iSubdataset), osDSDesc);
    }

    if (oSubdatasetList.Count() > 0)
    {
        // Afegir al metadades del dataset principal
        SetMetadata(oSubdatasetList.List(), "SUBDATASETS");
        oSubdatasetList.Clear();
        bMetadataDirty = false;
    }
}

void MMRDataset::AssignBands()
{
    for (int nIBand = 0; nIBand < hMMR->nBands; nIBand++)
    {
        if (!hMMR->papoBand[nIBand])
            continue;  // It's impoosible, but...

        // Establish raster info.
        nRasterXSize = hMMR->papoBand[nIBand]->nWidth;
        nRasterYSize = hMMR->papoBand[nIBand]->nHeight;
        GetBandBoundingBox(nIBand);  // Fills adfGeoTransform for this band(s)

        SetBand(nBands + 1, new MMRRasterBand(this, nBands + 1));

        MMRRasterBand *poBand =
            static_cast<MMRRasterBand *>(GetRasterBand(nIBand + 1));

        if (!hMMR->papoBand[nIBand]->GetFriendlyDescription().empty())
        {
            poBand->SetMetadataItem(
                "DESCRIPTION",
                hMMR->papoBand[nIBand]->GetFriendlyDescription());
        }

        // Collect GDAL custom Metadata, and "auxiliary" metadata from
        // well known MMR structures for the bands.  We defer this till
        // now to ensure that the bands are properly setup before
        // interacting with PAM.
        //·$·TODO ens saltem aixo de moment.

        /*char**papszMD = MMRGetMetadata(hMMR, i + 1);
        if (papszMD != nullptr)
        {
            poBand->SetMetadata(papszMD);
            CSLDestroy(papszMD);
        }*/

        //poBand->ReadAuxMetadata();
        //poBand->ReadHistogramMetadata();
    }

    /*
    // Check for GDAL style metadata.
    char**papszMD = MMRGetMetadata(hMMR, 0);
    if (papszMD != nullptr)
    {
        SetMetadata(papszMD);
        CSLDestroy(papszMD);
    }

    // Read the elevation metadata, if present.
    for (int iBand = 0; iBand < nBands; iBand++)
    {
        MMRRasterBand *poBand =
            static_cast<MMRRasterBand *>(GetRasterBand(iBand + 1));
        const char* pszEU = MMRReadElevationUnit(hMMR, iBand);

        if (pszEU != nullptr)
        {
            poBand->SetUnitType(pszEU);
            if (nBands == 1)
            {
                SetMetadataItem("ELEVATION_UNITS", pszEU);
            }
        }
    }
    */

    // Clear dirty metadata flags.
    for (int i = 0; i < nBands; i++)
    {
        MMRRasterBand *poBand =
            static_cast<MMRRasterBand *>(GetRasterBand(i + 1));
        poBand->bMetadataDirty = false;
    }
    bMetadataDirty = false;
}
