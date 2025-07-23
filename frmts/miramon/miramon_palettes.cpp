/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRPalettes class.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "miramon_rel.h"
#include "miramon_palettes.h"

#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()

MMRPalettes::MMRPalettes(MMRRel &fRel, CPLString osBandSectionIn)
    : pfRel(&fRel), osBandSection(osBandSectionIn)
{
    CPLString os_Color_Paleta = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Paleta");

    if (os_Color_Paleta.empty() || os_Color_Paleta == "<Automatic>")
    {
        bIsValid = true;
        return;
    }

    CPLString osExtension = CPLGetExtensionSafe(os_Color_Paleta);
    if (osExtension.tolower() == "dbf")
    {
        if (CE_None != GetPaletteColors_DBF(os_Color_Paleta))
            return;

        bIsValid = true;
        return;
    }
    if (osExtension.tolower() == "pal" || osExtension.tolower() == "p25" ||
        osExtension.tolower() == "p65")
    {
        if (CE_None != GetPaletteColors_PAL_P25_P65(os_Color_Paleta))
            return;

        bIsValid = true;
        return;
    }
}

MMRPalettes::~MMRPalettes()
{
}

void MMRPalettes::AssignColorFromDBF(struct MM_DATA_BASE_XP &oColorTable,
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
        // Transparent (white or whatever color)
        aadfPaletteColors[0][nIPaletteIndex] = sNoDataColorRGB.c1;
        aadfPaletteColors[1][nIPaletteIndex] = sNoDataColorRGB.c2;
        aadfPaletteColors[2][nIPaletteIndex] = sNoDataColorRGB.c3;
        aadfPaletteColors[3][nIPaletteIndex] = sNoDataColorRGB.c4;
    }
    else
        aadfPaletteColors[3][nIPaletteIndex] = 255;
}

CPLErr MMRPalettes::GetPaletteColors_DBF_Indexs(
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

CPLErr MMRPalettes::GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF)

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

    if (bIsCategorical)
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
                bHasNodata = true;
            else if (nPCTColors < atoi(osField))
                nPCTColors = atoi(osField);
        }
        nPCTColors++;  // Number is one more than the maximum

        // If there is nodata color, it has to be computed
        if (bHasNodata)
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
    if (bIsCategorical)
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
                bHasNodata = true;
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

// Colors in a PAL, P25 or P65 format files
CPLErr MMRPalettes::GetPaletteColors_PAL_P25_P65(CPLString os_Color_Paleta_DBF)

{
    CPLString osAux =
        CPLGetPathSafe(static_cast<const char *>(pfRel->GetRELNameChar()));
    CPLString osColorTableFileName =
        CPLFormFilenameSafe(osAux.c_str(), os_Color_Paleta_DBF.c_str(), "");

    // This kind of palette has not NoData color.
    //bHasNodata = false;

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
