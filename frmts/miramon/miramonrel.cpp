/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRREL class.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_priv.h"

#include "miramon_p.h"

#ifdef MSVC
#include "..\miramon_common\mm_gdal_functions.h"  // For MMCheck_REL_FILE()
//#include "..\miramon_common\mm_gdal_constants.h"
#else
#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()
//#include "../miramon_common/mm_gdal_constants.h"
#endif

#include "miramon_rastertools.h"

/************************************************************************/
/*                              MMRRel()                               */
/************************************************************************/

MMRRel::MMRRel(CPLString osRELFilenameIn) : osRelFileName(osRELFilenameIn)
{
}

/************************************************************************/
/*                              ~MMRRel()                              */
/************************************************************************/

MMRRel::~MMRRel()
{
}

/************************************************************************/
/*                              SetInfoFromREL()                        */
/************************************************************************/

CPLErr MMRRel::SetInfoFromREL(const char *pszFileName, MMRInfo &hMMR)

{
    CPLString osRELFileNameIn;

    // Getting the name of the REL
    const CPLString osMMRPrefix = "MiraMonRaster:";
    if (STARTS_WITH(pszFileName, osMMRPrefix))
    {
        // SUBDATASET case: gets the names of the bands in the subdataset
        CPLString osFilename = pszFileName;
        size_t nPos = osFilename.ifind(osMMRPrefix);
        if (nPos != 0)
            return CE_Failure;

        CPLString osSDSReL = osFilename.substr(osMMRPrefix.size());

        // Getting the internal names of the bands
        char **papszTokens = CSLTokenizeString2(osSDSReL, ",", 0);
        const int nTokens = CSLCount(papszTokens);

        if (nTokens < 1)
            return CE_Failure;

        osRELFileNameIn = papszTokens[0];
        osRELFileNameIn.replaceAll("\"", "");

        // Getting the list of bands in the subdataset
        for (int nIBand = 0; nIBand < nTokens - 1; nIBand++)
        {
            // Raw band name
            CPLString osBandName = papszTokens[nIBand + 1];
            osBandName.replaceAll("\"", "");
            hMMR.papoSDSBands.emplace_back(osBandName);
        }
        CSLDestroy(papszTokens);
    }
    else
    {
        // Getting the metadata file name. If it's already a REL file,
        // then same name is returned.
        osRELFileNameIn = MMRRel::GetAssociatedMetadataFileName(
            pszFileName, hMMR.bIsAMiraMonFile);
        if (osRELFileNameIn.empty())
        {
            if (hMMR.bIsAMiraMonFile)
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Metadata file for %s should exist.", pszFileName);
            }
            return CE_Failure;
        }
    }

    // If rel name was not a REL name, we update that
    // from the one found from IMG file.
    SetRELNameChar(osRELFileNameIn);

    hMMR.fRel = this;
    hMMR.osRELFileName = osRELFileNameIn;

    // Collect band definitions.
    if (ParseBandInfo(hMMR) != CE_None)
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*              GetAssociatedMetadataFileName()                      */
/************************************************************************/

// Converts FileName.img to FileNameI.rel
CPLString MMRRel::MMRGetSimpleMetadataName(const char *pszLayerName)
{
    if (!pszLayerName)
        return "";

    // Extract extension
    CPLString pszRELFile =
        CPLString(CPLResetExtensionSafe(pszLayerName, "").c_str());

    if (!pszRELFile.length())
        return "";

    // Extract "."
    pszRELFile.resize(pszRELFile.size() - 1);
    // Add "I.rel"
    pszRELFile += pszExtRasterREL;

    return pszRELFile;
}

CPLString MMRRel::GetMetadataValueDirectly(const char *pszRELFile,
                                           const char *pszSection,
                                           const char *pszKey)
{
    char *pszValue =
        MMReturnValueFromSectionINIFile(pszRELFile, pszSection, pszKey);
    if (!pszValue)
        return "";
    else
    {
        CPLString osValue = pszValue;
        CPLFree(pszValue);
        return osValue;
    }
}

// Gets the state (enum class MMRNomFitxerState) of NomFitxer in the
// specified section
// [pszSection]
// NomFitxer=Value
MMRNomFitxerState MMRRel::MMRStateOfNomFitxerInSection(const char *pszLayerName,
                                                       const char *pszSection,
                                                       const char *pszRELFile)
{
    CPLString osDocumentedLayerName =
        GetMetadataValueDirectly(pszRELFile, pszSection, KEY_NomFitxer);

    if (osDocumentedLayerName.empty())
        return MMRNomFitxerState::NOMFITXER_NOT_FOUND;

    if (*osDocumentedLayerName == '\0')
        return MMRNomFitxerState::NOMFITXER_VALUE_EMPTY;

    CPLString pszFileAux = CPLFormFilenameSafe(
        CPLGetPathSafe(pszRELFile).c_str(), osDocumentedLayerName, "");

    osDocumentedLayerName =
        RemoveWhitespacesFromEndOfString(osDocumentedLayerName);
    if (*osDocumentedLayerName == '*' || *osDocumentedLayerName == '?')
        return MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED;

    // Is the found Value the same than the pszLayerName file?
    if (EQUAL(pszFileAux, pszLayerName))
    {
        return MMRNomFitxerState::NOMFITXER_VALUE_EXPECTED;
    }

    return MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED;
}

// Tries to find a reference to the IMG file 'pszLayerName'
// we are opening in the REL file 'pszRELFile'
CPLString MMRRel::MMRGetAReferenceToIMGFile(const char *pszLayerName,
                                            const char *pszRELFile,
                                            bool &bIsAMiraMonFile)
{
    if (!pszRELFile)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Expected File name.");
        return "";
    }

    // [ATTRIBUTE_DATA]
    // NomFitxer=
    // It should be empty but if it's not, at least,
    // the value has to be pszLayerName
    MMRNomFitxerState iState = MMRStateOfNomFitxerInSection(
        pszLayerName, SECTION_ATTRIBUTE_DATA, pszRELFile);

    if (iState == MMRNomFitxerState::NOMFITXER_VALUE_EXPECTED ||
        iState == MMRNomFitxerState::NOMFITXER_VALUE_EMPTY)
    {
        return pszRELFile;
    }
    else if (iState == MMRNomFitxerState::NOMFITXER_VALUE_UNEXPECTED)
    {
        if (bIsAMiraMonFile)
        {
            CPLError(
                CE_Failure, CPLE_OpenFailed,
                "Unexpected value for SECTION_ATTRIBUTE_DATA [NomFitxer] in "
                "%s file.",
                pszRELFile);
        }
        return "";
    }

    // Discarting not supported via SDE (some files
    // could have this otpion)
    CPLString osVia =
        GetMetadataValueDirectly(pszRELFile, SECTION_ATTRIBUTE_DATA, KEY_via);
    if (!osVia.empty() && !EQUAL(osVia, "SDE"))
    {
        if (bIsAMiraMonFile)
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "Unexpected Via in %s file",
                     pszRELFile);
        }
        return "";
    }

    CPLString osFieldNames = GetMetadataValueDirectly(
        pszRELFile, SECTION_ATTRIBUTE_DATA, Key_IndexsNomsCamps);

    if (osFieldNames.empty())
    {
        if (bIsAMiraMonFile)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "IndexsNomsCamps not found in %s file", pszRELFile);
        }
        return "";
    }

    // Getting the internal names of the bands
    char **papszTokens = CSLTokenizeString2(osFieldNames, ",", 0);
    const int nBands = CSLCount(papszTokens);

    CPLString osBandSectionKey;
    CPLString osAtributeDataName;
    for (int nIBand = 0; nIBand < nBands; nIBand++)
    {
        osBandSectionKey = KEY_NomCamp;
        osBandSectionKey.append("_");
        osBandSectionKey.append(papszTokens[nIBand]);

        CPLString osBandSectionValue = GetMetadataValueDirectly(
            pszRELFile, SECTION_ATTRIBUTE_DATA, osBandSectionKey);

        if (!osBandSectionValue)
            continue;  // A band without name (·$· unexpected)

        char *pszBandSectionValue = CPLStrdup(osBandSectionValue);
        MM_RemoveWhitespacesFromEndOfString(pszBandSectionValue);
        osBandSectionValue = pszBandSectionValue;
        VSIFree(pszBandSectionValue);

        // Example: [ATTRIBUTE_DATA:G1]
        osAtributeDataName = SECTION_ATTRIBUTE_DATA;
        osAtributeDataName.append(":");
        osAtributeDataName.append(osBandSectionValue);

        // Let's see if this band contains the expected name
        // or none (in monoband case)
        iState = MMRStateOfNomFitxerInSection(
            pszLayerName, osAtributeDataName.c_str(), pszRELFile);
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
    if (bIsAMiraMonFile)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "REL search failed for all bands in %s file", pszRELFile);
    }
    return "";
}

// Finds the metadata filename associated to pszFileName (usually an IMG file)
CPLString MMRRel::GetAssociatedMetadataFileName(const char *pszFileName,
                                                bool &bIsAMiraMonFile)
{
    if (!pszFileName)
    {
        if (bIsAMiraMonFile)
            CPLError(CE_Failure, CPLE_OpenFailed, "Expected File name.");
        return "";
    }

    // If the string finishes in "I.rel" we consider it can be
    // the associated file to all bands that are documented in this file.
    if (strlen(pszFileName) >= strlen(pszExtRasterREL) &&
        EQUAL(pszFileName + strlen(pszFileName) - strlen(pszExtRasterREL),
              pszExtRasterREL))
    {
        bIsAMiraMonFile = true;
        return CPLString(pszFileName);
    }

    // If the file is not a REL file, let's try to find the associated REL
    // It must be a IMG file.
    CPLString pszExtension =
        CPLString(CPLGetExtensionSafe(pszFileName).c_str());
    if (!EQUAL(pszExtension, pszExtRaster + 1))
        return "";

    // Converting FileName.img to FileNameI.rel
    CPLString osRELFile = MMRGetSimpleMetadataName(pszFileName);
    if (osRELFile.empty())
    {
        if (bIsAMiraMonFile)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failing in conversion from .img to I.rel for %s file",
                     pszFileName);
        }
        return "";
    }

    // Checking if the file exists
    VSIStatBufL sStat;
    if (VSIStatExL(osRELFile.c_str(), &sStat, VSI_STAT_EXISTS_FLAG) == 0)
        return MMRGetAReferenceToIMGFile(pszFileName, osRELFile.c_str(),
                                         bIsAMiraMonFile);

    const CPLString osPath = CPLGetPathSafe(pszFileName);
    char **folder = VSIReadDir(osPath.c_str());
    int size = folder ? CSLCount(folder) : 0;

    for (int i = 0; i < size; i++)
    {
        if (folder[i][0] == '.' || !strstr(folder[i], "I.rel"))
        {
            continue;
        }

        const CPLString osFilePath =
            CPLFormFilenameSafe(osPath, folder[i], nullptr);

        osRELFile = MMRGetAReferenceToIMGFile(pszFileName, osFilePath.c_str(),
                                              bIsAMiraMonFile);
        if (!osRELFile.empty())
        {
            CSLDestroy(folder);
            return osRELFile;
        }
    }

    CSLDestroy(folder);
    if (bIsAMiraMonFile)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "REL search failed for %s file",
                 pszFileName);
    }

    return "";
}

/************************************************************************/
/*                         CheckBandInRel()                             */
/************************************************************************/
CPLErr MMRRel::CheckBandInRel(const char *pszRELFileName,
                              const char *pszIMGFile)

{
    CPLString osFieldNames = GetMetadataValueDirectly(
        pszRELFileName, SECTION_ATTRIBUTE_DATA, Key_IndexsNomsCamps);

    if (osFieldNames.empty())
        return CE_Failure;

    // Separator ,
    char **papszTokens = CSLTokenizeString2(osFieldNames, ",", 0);
    const int nTokenCount = CSLCount(papszTokens);

    if (!nTokenCount)
        return CE_Failure;

    CPLString osBandSectionKey;
    CPLString pszFileAux;
    CPLString osBandSectionValue;
    for (int i = 0; i < nTokenCount; i++)
    {
        osBandSectionKey = KEY_NomCamp;
        osBandSectionKey.append("_");
        osBandSectionKey.append(papszTokens[i]);

        osBandSectionValue = GetMetadataValueDirectly(
            pszRELFileName, SECTION_ATTRIBUTE_DATA, osBandSectionKey);

        if (osBandSectionValue.empty())
            return CE_Failure;

        RemoveWhitespacesFromEndOfString(osBandSectionValue);

        CPLString osAtributeDataName;
        osAtributeDataName = SECTION_ATTRIBUTE_DATA;
        osAtributeDataName.append(":");
        osAtributeDataName.append(osBandSectionValue);

        CPLString osRawBandFileName = GetMetadataValueDirectly(
            pszRELFileName, osAtributeDataName, KEY_NomFitxer);

        if (osRawBandFileName.empty())
        {
            CPLString osBandFileName =
                MMRGetFileNameFromRelName(pszRELFileName);
            if (osBandFileName.empty())
                return CE_Failure;
        }
        else
        {
            if (!EQUAL(osRawBandFileName, pszIMGFile))
                continue;
            break;  // Found
        }
    }

    CSLDestroy(papszTokens);

    return CE_None;
}

/************************************************************************/
/*              IdentifySubdataSetFile()                             */
/************************************************************************/
int MMRRel::IdentifySubdataSetFile(const CPLString pszFileName)
{
    const CPLString osMMRPrefix = "MiraMonRaster:";
    if (!STARTS_WITH(pszFileName, osMMRPrefix))
        return FALSE;

    // SUBDATASETS
    CPLString osFilename = pszFileName;
    size_t nPos = osFilename.ifind(osMMRPrefix);
    if (nPos != 0)
        return GDAL_IDENTIFY_FALSE;

    CPLString osRELAndBandName = osFilename.substr(osMMRPrefix.size());

    char **papszTokens = CSLTokenizeString2(osRELAndBandName, ",", 0);
    const int nTokens = CSLCount(papszTokens);
    // Getting the REL associated to the bands
    // We need the REL and at least one band (index + name).
    if (nTokens < 2)
        return GDAL_IDENTIFY_FALSE;

    // Let's remove "\"" if existant.
    CPLString osRELName = papszTokens[0];
    osRELName.replaceAll("\"", "");

    // It must be a I.rel file.
    if (!(strlen(osRELName) >= strlen(pszExtRasterREL) &&
          EQUAL(osRELName + strlen(osRELName) - strlen(pszExtRasterREL),
                pszExtRasterREL)))
        return GDAL_IDENTIFY_FALSE;

    if (MMCheck_REL_FILE(osRELName))
        return GDAL_IDENTIFY_FALSE;

    // Let's see if the specifieds bands are in the REL file
    // Getting the index + internal names of the bands
    for (int nIBand = 1; nIBand < nTokens; nIBand++)
    {
        // Let's check that this band (papszTokens[nIBand]) is in the REL file.
        CPLString osBandName = papszTokens[nIBand];

        // If it's not an IMG file return FALSE
        CPLString pszExtension =
            CPLString(CPLGetExtensionSafe(osBandName).c_str());
        if (!EQUAL(pszExtension, pszExtRaster + 1))
            return GDAL_IDENTIFY_FALSE;

        // Let's remove "\"" if existant.
        osBandName.replaceAll("\"", "");
        if (CE_None != CheckBandInRel(osRELName, osBandName))
        {
            CSLDestroy(papszTokens);
            return GDAL_IDENTIFY_FALSE;
        }
    }
    CSLDestroy(papszTokens);
    return GDAL_IDENTIFY_TRUE;
}

/************************************************************************/
/*                     IdentifyFile()                                */
/************************************************************************/
int MMRRel::IdentifyFile(GDALOpenInfo *poOpenInfo)
{
    // IMG files are shared for many drivers.
    // Identify will mark it as unknown.
    // Open function will try to open that, but as it has computation
    // cost is better avoid doing it here.
    if (poOpenInfo->IsExtensionEqualToCI("IMG"))
        return GDAL_IDENTIFY_UNKNOWN;

    if (!poOpenInfo->IsExtensionEqualToCI("REL"))
        return GDAL_IDENTIFY_FALSE;

    // In fact, the file has to end with I.rel (pszExtRasterREL)
    if (!(strlen(poOpenInfo->pszFilename) >= strlen(pszExtRasterREL) &&
          EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename) -
                    strlen(pszExtRasterREL),
                pszExtRasterREL)))
        return GDAL_IDENTIFY_FALSE;

    // Some versions of REL files are not allowed.
    if (MMCheck_REL_FILE(poOpenInfo->pszFilename))
        return GDAL_IDENTIFY_FALSE;

    return GDAL_IDENTIFY_TRUE;
}

/************************************************************************/
/*                  GetDataTypeAndBytesPerPixel()                     */
/************************************************************************/
int MMRRel::GetDataTypeAndBytesPerPixel(const char *pszCompType,
                                        MMDataType *nCompressionType,
                                        MMBytesPerPixel *nBytesPerPixel)
{
    if (!nCompressionType || !nBytesPerPixel || !pszCompType)
        return 1;

    if (EQUAL(pszCompType, "bit"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BIT;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "byte"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BYTE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "byte-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_BYTE_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "integer"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_INTEGER;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "integer-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "uinteger"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_UINTEGER;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "uinteger-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "long"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_LONG;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "long-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_LONG_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "real"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_REAL;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "real-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_REAL_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "double"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_DOUBLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
        return 0;
    }
    if (EQUAL(pszCompType, "double-RLE"))
    {
        *nCompressionType = MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE;
        *nBytesPerPixel = MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE;
        return 0;
    }

    return 1;
}

/************************************************************************/
/*                     GetMetadataValue()                               */
/************************************************************************/
CPLString MMRRel::GetMetadataValue(const char *pszMainSection,
                                   const char *pszSubSection,
                                   const char *pszSubSubSection,
                                   const char *pszKey)
{
    // Searches in [pszMainSection:pszSubSection]
    CPLString osAtributeDataName;
    osAtributeDataName = pszMainSection;
    osAtributeDataName.append(":");
    osAtributeDataName.append(pszSubSection);
    osAtributeDataName.append(":");
    osAtributeDataName.append(pszSubSubSection);

    char *pszValue = MMReturnValueFromSectionINIFile(
        GetRELNameChar(), osAtributeDataName, pszKey);
    if (pszValue)
    {
        CPLString osValue = pszValue;
        CPLFree(pszValue);
        return osValue;
    }

    // If the value is not found then searches in [pszMainSection]
    pszValue = MMReturnValueFromSectionINIFile(GetRELNameChar(),
                                               pszSubSubSection, pszKey);
    if (pszValue)
    {
        CPLString osValue = pszValue;
        CPLFree(pszValue);
        return osValue;
    }
    return "";
}

CPLString MMRRel::GetMetadataValue(const char *pszMainSection,
                                   const char *pszSubSection,
                                   const char *pszKey)
{
    // Searches in [pszMainSection:pszSubSection]
    CPLString osAtributeDataName;
    osAtributeDataName = pszMainSection;
    osAtributeDataName.append(":");
    osAtributeDataName.append(pszSubSection);

    char *pszValue = MMReturnValueFromSectionINIFile(
        GetRELNameChar(), osAtributeDataName, pszKey);
    if (pszValue)
    {
        CPLString osValue = pszValue;
        CPLFree(pszValue);
        return osValue;
    }

    // If the value is not found then searches in [pszMainSection]
    pszValue = MMReturnValueFromSectionINIFile(GetRELNameChar(), pszMainSection,
                                               pszKey);
    if (pszValue)
    {
        CPLString osValue = pszValue;
        CPLFree(pszValue);
        return osValue;
    }
    return "";
}

CPLString MMRRel::GetMetadataValue(const char *pszSection, const char *pszKey)
{
    char *pszValue =
        MMReturnValueFromSectionINIFile(GetRELNameChar(), pszSection, pszKey);
    if (pszValue)
    {
        CPLString osValue = pszValue;
        CPLFree(pszValue);
        return osValue;
    }
    return "";
}

const char *MMRRel::GetRELNameChar()
{
    return osRelFileName.c_str();
}

void MMRRel::SetRELNameChar(CPLString osRelFileNameIn)
{
    osRelFileName = osRelFileNameIn;
}

/************************************************************************/
/*                          ParseBandInfo()                             */
/************************************************************************/
CPLErr MMRRel::ParseBandInfo(MMRInfo &hMMR)
{
    if (!hMMR.fRel)
        return CE_Fatal;

    hMMR.nBands = 0;

    CPLString pszRELFileName = hMMR.osRELFileName;

    CPLString osFieldNames = hMMR.fRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA,
                                                         Key_IndexsNomsCamps);

    if (osFieldNames.empty())
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "%s-%s section-key should exist in %s.",
                 SECTION_ATTRIBUTE_DATA, Key_IndexsNomsCamps,
                 pszRELFileName.c_str());
        return CE_Failure;
    }

    // Separator ,
    char **papszTokens = CSLTokenizeString2(osFieldNames, ",", 0);
    const int nTokenCount = CSLCount(papszTokens);

    if (!nTokenCount)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, "No bands in file %s.",
                 pszRELFileName.c_str());
        return CE_Failure;
    }

    CPLString osBandSectionKey;
    CPLString pszFileAux;
    CPLString osBandSectionValue;

    int nNBand;
    if (hMMR.papoSDSBands.size())
        nNBand = static_cast<int>(hMMR.papoSDSBands.size());
    else
        nNBand = nTokenCount;

    hMMR.papoBand = new MMRBand *[nNBand];

    for (int i = 0; i < nTokenCount; i++)
    {
        osBandSectionKey = KEY_NomCamp;
        osBandSectionKey.append("_");
        osBandSectionKey.append(papszTokens[i]);

        osBandSectionValue = hMMR.fRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA,
                                                         osBandSectionKey);

        if (!osBandSectionValue)
            continue;

        if (hMMR.papoSDSBands.size())
        {
            CPLString osRawBandFileName = hMMR.fRel->GetMetadataValue(
                SECTION_ATTRIBUTE_DATA, osBandSectionValue, KEY_NomFitxer);

            // I'm in a Subataset
            size_t nISDSBand;
            for (nISDSBand = 0; nISDSBand < hMMR.papoSDSBands.size();
                 nISDSBand++)
            {
                if (hMMR.papoSDSBands[nISDSBand] == osRawBandFileName)
                    break;
            }
            if (nISDSBand == hMMR.papoSDSBands.size())
                continue;
        }

        osBandSectionValue =
            RemoveWhitespacesFromEndOfString(osBandSectionValue);

        hMMR.papoBand[static_cast<size_t>(hMMR.nBands)] =
            new MMRBand(hMMR, osBandSectionValue);

        if (hMMR.papoBand[static_cast<size_t>(hMMR.nBands)]->nWidth == 0)
        {
            CSLDestroy(papszTokens);
            return CE_Failure;
        }

        hMMR.papoBand[static_cast<size_t>(hMMR.nBands)]->SetRELFileName(
            pszRELFileName);
        hMMR.nBands++;
    }

    CSLDestroy(papszTokens);

    return CE_None;
}

// ·$·TODO USE ONLY CPL stuff
CPLString MMRRel::RemoveWhitespacesFromEndOfString(CPLString osInputWithSpaces)
{
    char *pszBandSectionValue = CPLStrdup(osInputWithSpaces);
    MM_RemoveWhitespacesFromEndOfString(pszBandSectionValue);
    osInputWithSpaces = pszBandSectionValue;
    VSIFree(pszBandSectionValue);
    return osInputWithSpaces;
}
