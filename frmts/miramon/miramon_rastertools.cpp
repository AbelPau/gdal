/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements some raster functions.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "miramon_p.h"

#include <cstddef>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_priv.h"

#ifdef MSVC
#include "..\miramon_common\mm_gdal_functions.h"
#include "..\miramon_common\mm_gdal_constants.h"
#else
#include "../miramon_common/mm_gdal_functions.h"
#include "../miramon_common/mm_gdal_constants.h"
#endif

constexpr auto pszExtRaster = ".img";
constexpr auto pszExtRasterREL = "I.rel";

// Converts FileName.img to FileNameI.rel
CPLString MMRGetSimpleMetadataName(const char *pszLayerName)
{
    if (!pszLayerName)
        return "";

    CPLString pszRELFile =
        CPLString(CPLResetExtensionSafe(pszLayerName, "").c_str());

    if (!pszRELFile.length())
        return "";

    pszRELFile.resize(pszRELFile.size() - 1);
    pszRELFile += pszExtRasterREL;

    return pszRELFile;
}

// Converts FileNameI.rel to FileName.img
CPLString MMRGetNameFromMetadata(const char *pszRELFile)
{
    if (!pszRELFile)
        return "";

    CPLString pszFile =
        CPLString(CPLResetExtensionSafe(pszRELFile, "").c_str());

    if (pszFile.length() < 2)
        return "";

    pszFile.resize(pszFile.size() - 2);  // I.
    pszFile += pszExtRaster;

    return pszFile;
}

enum class MMRNomFitxerState
{
    NOMFITXER_NOT_FOUND,        // There is no NomFitxer key
    NOMFITXER_VALUE_EXPECTED,   // The NomFitxer value is the expected
    NOMFITXER_VALUE_EMPTY,      // The NomFitxer value is empty
    NOMFITXER_VALUE_UNEXPECTED  // The NomFitxer value is unexpected
};

MMRNomFitxerState MMRStateOfNomFitxerInSection(const char *pszLayerName,
                                               const char *pszSection,
                                               const char *pszRELFile)
{
    // Gets the state of NomFitxer in specified section
    // [pszSection]
    // NomFitxer=Value
    char *pszDocumentedLayerName =
        MMReturnValueFromSectionINIFile(pszRELFile, pszSection, KEY_NomFitxer);

    if (!pszDocumentedLayerName)
    {
        return MMRNomFitxerState::NOMFITXER_NOT_FOUND;
    }

    if (*pszDocumentedLayerName == '\0')
    {
        return MMRNomFitxerState::NOMFITXER_VALUE_EMPTY;
    }
    CPLString pszFileAux = CPLFormFilenameSafe(
        CPLGetPathSafe(pszRELFile).c_str(), pszDocumentedLayerName, "");

    MM_RemoveWhitespacesFromEndOfString(pszDocumentedLayerName);
    if (*pszDocumentedLayerName == '*' || *pszDocumentedLayerName == '?')
    {
        return MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED;
    }

    CPLFree(pszDocumentedLayerName);

    // Is the found Value the same than the pszLayerName file?
    if (pszFileAux == pszLayerName)
    {
        return MMRNomFitxerState::NOMFITXER_VALUE_EXPECTED;
    }

    return MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED;
}

CPLString MMRGetGenericFieldFilesFromREL(const char *pszLayerName,
                                         const char *pszRELFile,
                                         bool *bMultiBand)
{
    if (bMultiBand)
        *bMultiBand = false;

    if (!pszRELFile)
        return "";

    // [ATTRIBUTE_DATA]
    // NomFitxer=
    // It should be empty but if it's not, at least,
    // the value has to be pszLayerName
    MMRNomFitxerState iState = MMRStateOfNomFitxerInSection(
        pszLayerName, SECCIO_ATTRIBUTE_DATA, pszRELFile);

    if (iState == MMRNomFitxerState::NOMFITXER_VALUE_EXPECTED ||
        iState == MMRNomFitxerState::NOMFITXER_VALUE_EMPTY)
    {
        return pszRELFile;
    }
    else if (iState == MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED)
    {
        return "";
    }

    // Discarting not supported via SDE (some files
    // could have this otpion)
    char *pszVia = MMReturnValueFromSectionINIFile(
        pszRELFile, SECCIO_ATTRIBUTE_DATA, KEY_via);
    if (pszVia && *pszVia != '\0' && !stricmp(pszVia, "SDE"))
    {
        VSIFree(pszVia);
        return "";
    }
    VSIFree(pszVia);

    char *pszFieldNames = MMReturnValueFromSectionINIFile(
        pszRELFile, SECCIO_ATTRIBUTE_DATA, Key_IndexsNomsCamps);

    // Getting the internal names of the bands
    char **papszTokens = CSLTokenizeString2(pszFieldNames, ",", 0);
    const int nBands = CSLCount(papszTokens);
    VSIFree(pszFieldNames);

    if (bMultiBand && nBands > 1)
        *bMultiBand = true;

    CPLString szFieldName;
    CPLString szAtributeDataName;
    for (size_t nIBand = 0; nIBand < nBands; nIBand++)
    {
        szFieldName = KEY_NomCamp;
        szFieldName.append("_");
        szFieldName.append(papszTokens[nIBand]);

        char *pszFieldNameValue = MMReturnValueFromSectionINIFile(
            pszRELFile, SECCIO_ATTRIBUTE_DATA, szFieldName);

        if (!pszFieldNameValue)
            continue;  // A band without name (·$· unexpected)

        MM_RemoveWhitespacesFromEndOfString(pszFieldNameValue);

        // Example: [ATTRIBUTE_DATA:G1]
        szAtributeDataName = SECCIO_ATTRIBUTE_DATA;
        szAtributeDataName.append(":");
        szAtributeDataName.append(pszFieldNameValue);

        VSIFree(pszFieldNameValue);

        // Let's see if this band contains the expected name
        // or none (in monoband case)
        iState = MMRStateOfNomFitxerInSection(
            pszLayerName, szAtributeDataName.c_str(), pszRELFile);
        if (iState == MMRNomFitxerState::NOMFITXER_VALUE_EXPECTED)
        {
            CSLDestroy(papszTokens);
            return pszRELFile;
        }
        else if (iState == MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED)
        {
            continue;
        }

        // If there is only one band is accepted NOMFITXER_NOT_FOUND/EMPTY iState result
        if (nBands == 1)
        {
            CSLDestroy(papszTokens);
            return pszRELFile;
        }
    }

    CSLDestroy(papszTokens);
    return "";
}

CPLString MMRGetAssociatedMetadataName(const char *pszLayerName,
                                       bool *bMultiBand)
{
    if (!pszLayerName)
        return "";

    CPLString pszExtension =
        CPLString(CPLGetExtensionSafe(pszLayerName).c_str());

    if (!EQUAL(pszExtension, pszExtRaster + 1))
        return "";

    // Converting FileName.img to FileNameI.rel
    CPLString pszRELFile = MMRGetSimpleMetadataName(pszLayerName);
    if (EQUAL(pszRELFile, ""))
    {
        if (bMultiBand)
            *bMultiBand = false;
        return "";
    }

    // Checking if the file exists
    VSIStatBufL sStat;
    if (VSIStatExL(pszRELFile.c_str(), &sStat, VSI_STAT_EXISTS_FLAG) == 0)
    {
        /*
            The file pszRELFile exists. This file can be:
            - the REL file for this layer (FileName.img to FileNameI.rel).
                    In none of the ATTRIBUTE_DATA (or ATTRIBUTE_DATA_X) sections
                    will there be any key `NomFitxer=...`
            - a multi-band REL for this band and others (in one of the
                    ATTRIBUTE_DATA_X sections, there will be
                    a key `NomFitxer=pszLayerName`)
            - a multi-band REL for other layers but not for this one!
                    In NONE of the ATTRIBUTE_DATA_X sections will there be
                    a key `NomFitxer=pszLayerName` (but there may be other
                    `NomFitxer=...` keys -> NO_DIRECT_REL_INVALID)
        */
        return MMRGetGenericFieldFilesFromREL(pszLayerName, pszRELFile.c_str(),
                                              bMultiBand);
    }

    const CPLString osPath = CPLGetPathSafe(pszLayerName);
    char **folder = VSIReadDir(osPath.c_str());
    int size = folder ? CSLCount(folder) : 0;

    for (int i = 0; i < size; i++)
    {
        if (folder[i][0] == '.')
        {
            continue;
        }

        if (!strstr(folder[i], "I.rel"))
        {
            continue;
        }

        const std::string filepath =
            CPLFormFilenameSafe(osPath, folder[i], nullptr);

        pszRELFile = MMRGetGenericFieldFilesFromREL(
            pszLayerName, filepath.c_str(), bMultiBand);
        if (!EQUAL(pszRELFile, ""))
        {
            CSLDestroy(folder);
            return pszRELFile;
        }
    }

    CSLDestroy(folder);

    return "";
}

int MMGetDataTypeAndBytesPerPixel(const char *pszCompType,
                                  int *nCompressionType, int *nBytesPerPixel)
{
    if (!nCompressionType || !nBytesPerPixel || !pszCompType)
        return 1;

    if (EQUAL(pszCompType, "bit"))
    {
        *nCompressionType = *nBytesPerPixel = DATATYPE_AND_COMPR_BIT;
        return 0;
    }
    if (EQUAL(pszCompType, "byte"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_BYTE;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "byte-RLE"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_BYTE_RLE;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "integer"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_INTEGER;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "integer-RLE"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_INTEGER_RLE;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "uinteger"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_UINTEGER;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "uinteger-RLE"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_UINTEGER_RLE;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "long"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_LONG;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "long-RLE"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_LONG_RLE;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "real"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_REAL;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "real-RLE"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_REAL_RLE;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "double"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_DOUBLE;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "double-RLE"))
    {
        *nCompressionType = DATATYPE_AND_COMPR_DOUBLE_RLE;
        *nBytesPerPixel = TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
        return 0;
    }

    return 1;
}
