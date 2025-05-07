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

/*
Example: this function finds G1,
    [ATTRIBUTE_DATA]
    IndexsNomsCamps=1
    NomCamp_1=G1

then in section [ATTRIBUTE_DATA:G1] finds G1_1_BYTE_2X3_6_CATEGS_DBF
    [ATTRIBUTE_DATA:G1]
    descriptor=Al·leluia 1
    min=0
    max=5
    IndexsJoinTaula=G1_1_BYTE_2X3_6_CATEGS_DBF
    JoinTaula_G1_1_BYTE_2X3_6_CATEGS_DBF=G1_1_BYTE_2X3_6_CATEGS_DBF

then in section [TAULA_G1_1_BYTE_2X3_6_CATEGS_DBF] finds byte_2x3_6_categs.rel
    [TAULA_G1_1_BYTE_2X3_6_CATEGS_DBF]
    NomFitxer=byte_2x3_6_categs.rel
*/
CPLString GetGenericFieldFilesFromREL(const char *pszLayerName,
                                      const char *pszRELFile, bool *bMultiBand)
{
    if (bMultiBand)
        *bMultiBand = false;

    if (!pszRELFile)
        return "";

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
    const int nTokenCount = CSLCount(papszTokens);
    VSIFree(pszFieldNames);

    if (bMultiBand && nTokenCount > 1)
        *bMultiBand = true;

    CPLString szFieldName;
    CPLString szAtributeDataName;
    CPLString pszFileAux;
    char *pszFieldNameValue;
    for (size_t i = 0; i < nTokenCount; i++)
    {
        szFieldName = KEY_NomCamp;
        szFieldName.append("_");
        szFieldName.append(papszTokens[i]);

        pszFieldNameValue = MMReturnValueFromSectionINIFile(
            pszRELFile, SECCIO_ATTRIBUTE_DATA, szFieldName);

        if (!pszFieldNameValue)
            continue;

        MM_RemoveWhitespacesFromEndOfString(pszFieldNameValue);

        // Example: [ATTRIBUTE_DATA:G1]
        szAtributeDataName = SECCIO_ATTRIBUTE_DATA;
        szAtributeDataName.append(":");
        szAtributeDataName.append(pszFieldNameValue);

        VSIFree(pszFieldNameValue);

        pszFieldNameValue = MMReturnValueFromSectionINIFile(
            pszRELFile, szAtributeDataName.c_str(), KEY_NomFitxer);

        if (!pszFieldNameValue)
        {
            // Singular case where there are no suitable keys
            if (nTokenCount == 1)
            {
                return pszRELFile;
            }

            continue;
        }

        MM_RemoveWhitespacesFromEndOfString(pszFieldNameValue);
        if (!pszFieldNameValue)
            continue;

        if (*pszFieldNameValue == '*' || *pszFieldNameValue == '?')
            continue;

        // If some NomFitxer=pszLayerName exists,
        // just check this is the one we are trying to open
        pszFileAux = CPLFormFilenameSafe(CPLGetPathSafe(pszRELFile).c_str(),
                                         pszFieldNameValue, "");

        if (pszFileAux == pszLayerName)
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
        return GetGenericFieldFilesFromREL(pszLayerName, pszRELFile.c_str(),
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

        pszRELFile = GetGenericFieldFilesFromREL(pszLayerName, filepath.c_str(),
                                                 bMultiBand);
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
