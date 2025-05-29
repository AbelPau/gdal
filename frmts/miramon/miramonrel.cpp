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
/*                              GetInfoFromREL()                        */
/************************************************************************/

MMRHandle MMRRel::GetInfoFromREL(const char *pszFileName, const char *pszAccess)

{
    CPLString osRELFileNameIn;

    // Create the MMRInfo_t.
    MMRInfo_t *psInfo =
        static_cast<MMRInfo_t *>(CPLCalloc(sizeof(MMRInfo_t), 1));

    if (!psInfo)
        return nullptr;

    // Getting the name of the REL
    const CPLString osMMRPrefix = "MiraMonRaster:";
    if (STARTS_WITH(pszFileName, osMMRPrefix))
    {
        // SUBDATASET case: gets the names of the bands in the subdataset
        CPLString osFilename = pszFileName;
        size_t nPos = osFilename.ifind(osMMRPrefix);
        if (nPos != 0)
            return nullptr;

        CPLString osSDSReL = osFilename.substr(osMMRPrefix.size());

        // Getting the internal names of the bands
        char **papszTokens = CSLTokenizeString2(osSDSReL, ",", 0);
        const int nTokens = CSLCount(papszTokens);

        if (nTokens < 1)
            return nullptr;

        osRELFileNameIn = papszTokens[0];
        osRELFileNameIn.erase(
            std::remove(osRELFileNameIn.begin(), osRELFileNameIn.end(), '\"'),
            osRELFileNameIn.end());

        // Getting the list of bands in the subdataset
        psInfo->nSDSBands = nTokens - 1;
        psInfo->papoSDSBand = static_cast<CPLString **>(
            CPLMalloc(sizeof(CPLString *) * psInfo->nSDSBands));

        for (int nIBand = 0; nIBand < psInfo->nSDSBands; nIBand++)
        {
            // Raw band name
            CPLString osBandName = papszTokens[nIBand + 1];
            osBandName.erase(
                std::remove(osBandName.begin(), osBandName.end(), '\"'),
                osBandName.end());
            psInfo->papoSDSBand[nIBand] = new CPLString(osBandName);
        }
        CSLDestroy(papszTokens);
    }
    else
    {
        // Getting the metadata file name. If it's already a REL file,
        // then same name is returned.
        osRELFileNameIn = MMRRel::GetAssociatedMetadataFileName(pszFileName);
        if (EQUAL(osRELFileNameIn, ""))
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "Metadata file for %s \
                     should exist.",
                     pszFileName);

            return nullptr;
        }
    }

    // If rel name was not a REL name, we update that
    // from the one found from IMG file.
    SetRELNameChar(osRELFileNameIn);

    psInfo->fRel = this;
    psInfo->osRELFileName = osRELFileNameIn;

    if (EQUAL(pszAccess, "r") || EQUAL(pszAccess, "rb"))
        psInfo->eAccess = MMRAccess::MMR_ReadOnly;
    else
        psInfo->eAccess = MMRAccess::MMR_Update;

    psInfo->bTreeDirty = false;

    // Collect band definitions.
    if (ParseBandInfo(psInfo) != CE_None)
    {
        MMRClose(psInfo);
        psInfo = nullptr;
        return nullptr;
    }

    return psInfo;
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

// Gets the state (enum class MMRNomFitxerState) of NomFitxer in the
// specified section
// [pszSection]
// NomFitxer=Value
MMRNomFitxerState MMRRel::MMRStateOfNomFitxerInSection(const char *pszLayerName,
                                                       const char *pszSection,
                                                       const char *pszRELFile)
{
    char *pszDocumentedLayerName =
        MMReturnValueFromSectionINIFile(pszRELFile, pszSection, KEY_NomFitxer);

    if (!pszDocumentedLayerName)
    {
        return MMRNomFitxerState::NOMFITXER_NOT_FOUND;
    }

    if (*pszDocumentedLayerName == '\0')
    {
        CPLFree(pszDocumentedLayerName);
        return MMRNomFitxerState::NOMFITXER_VALUE_EMPTY;
    }
    CPLString pszFileAux = CPLFormFilenameSafe(
        CPLGetPathSafe(pszRELFile).c_str(), pszDocumentedLayerName, "");

    MM_RemoveWhitespacesFromEndOfString(pszDocumentedLayerName);
    if (*pszDocumentedLayerName == '*' || *pszDocumentedLayerName == '?')
    {
        CPLFree(pszDocumentedLayerName);
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

// Tries to find a reference to the IMG file 'pszLayerName'
// we are opening in the REL file 'pszRELFile'
CPLString MMRRel::MMRGetAReferenceToIMGFile(const char *pszLayerName,
                                            const char *pszRELFile)
{
    if (!pszRELFile)
        return "";

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
        return "";
    }

    // Discarting not supported via SDE (some files
    // could have this otpion)
    char *pszVia = MMReturnValueFromSectionINIFile(
        pszRELFile, SECTION_ATTRIBUTE_DATA, KEY_via);
    if (pszVia && *pszVia != '\0' && !EQUAL(pszVia, "SDE"))
    {
        VSIFree(pszVia);
        return "";
    }
    VSIFree(pszVia);

    char *pszFieldNames = MMReturnValueFromSectionINIFile(
        pszRELFile, SECTION_ATTRIBUTE_DATA, Key_IndexsNomsCamps);

    // Getting the internal names of the bands
    char **papszTokens = CSLTokenizeString2(pszFieldNames, ",", 0);
    const int nBands = CSLCount(papszTokens);
    VSIFree(pszFieldNames);

    CPLString pszBandSectionKey;
    CPLString szAtributeDataName;
    for (int nIBand = 0; nIBand < nBands; nIBand++)
    {
        pszBandSectionKey = KEY_NomCamp;
        pszBandSectionKey.append("_");
        pszBandSectionKey.append(papszTokens[nIBand]);

        char *pszBandSectionValue = MMReturnValueFromSectionINIFile(
            pszRELFile, SECTION_ATTRIBUTE_DATA, pszBandSectionKey);

        if (!pszBandSectionValue)
            continue;  // A band without name (·$· unexpected)

        MM_RemoveWhitespacesFromEndOfString(pszBandSectionValue);

        // Example: [ATTRIBUTE_DATA:G1]
        szAtributeDataName = SECTION_ATTRIBUTE_DATA;
        szAtributeDataName.append(":");
        szAtributeDataName.append(pszBandSectionValue);

        VSIFree(pszBandSectionValue);

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

// Finds the metadata filename associated to pszFileName (usually an IMG file)
CPLString MMRRel::GetAssociatedMetadataFileName(const char *pszFileName)
{
    if (!pszFileName)
        return "";

    // If the string finishes in "I.rel" we consider it can be
    // the associated file to all bands that are documented in this file.
    if (strlen(pszFileName) >= strlen(pszExtRasterREL) &&
        EQUAL(pszFileName + strlen(pszFileName) - strlen(pszExtRasterREL),
              pszExtRasterREL))
    {
        return CPLString(pszFileName);
    }

    // If the file is not a REL file, let's try to find the associated REL
    // It must be a IMG file.
    CPLString pszExtension =
        CPLString(CPLGetExtensionSafe(pszFileName).c_str());
    if (!EQUAL(pszExtension, pszExtRaster + 1))
    {
        return "";
    }

    // Converting FileName.img to FileNameI.rel
    CPLString pszRELFile = MMRGetSimpleMetadataName(pszFileName);
    if (EQUAL(pszRELFile, ""))
    {
        return "";
    }

    // Checking if the file exists
    VSIStatBufL sStat;
    if (VSIStatExL(pszRELFile.c_str(), &sStat, VSI_STAT_EXISTS_FLAG) == 0)
    {
        return MMRGetAReferenceToIMGFile(pszFileName, pszRELFile.c_str());
    }

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

        pszRELFile = MMRGetAReferenceToIMGFile(pszFileName, osFilePath.c_str());
        if (!EQUAL(pszRELFile, ""))
        {
            CSLDestroy(folder);
            return pszRELFile;
        }
    }

    CSLDestroy(folder);

    return "";
}

/************************************************************************/
/*                         CheckBandInRel()                             */
/************************************************************************/
CPLErr MMRRel::CheckBandInRel(const char *pszRELFileName,
                              const char *pszIMGFile)

{
    char *pszFieldNames = MMReturnValueFromSectionINIFile(
        pszRELFileName, SECTION_ATTRIBUTE_DATA, Key_IndexsNomsCamps);

    if (!pszFieldNames)
        return CE_Failure;

    // Separator ,
    char **papszTokens = CSLTokenizeString2(pszFieldNames, ",", 0);
    const int nTokenCount = CSLCount(papszTokens);

    if (!nTokenCount)
    {
        VSIFree(pszFieldNames);
        return CE_Failure;
    }

    CPLString pszBandSectionKey;
    CPLString pszFileAux;
    char *pszBandSectionValue;
    for (int i = 0; i < nTokenCount; i++)
    {
        pszBandSectionKey = KEY_NomCamp;
        pszBandSectionKey.append("_");
        pszBandSectionKey.append(papszTokens[i]);

        pszBandSectionValue = MMReturnValueFromSectionINIFile(
            pszRELFileName, SECTION_ATTRIBUTE_DATA, pszBandSectionKey);

        if (!pszBandSectionValue)
        {
            VSIFree(pszBandSectionValue);
            VSIFree(pszFieldNames);
            return CE_Failure;
        }

        MM_RemoveWhitespacesFromEndOfString(pszBandSectionValue);

        CPLString szAtributeDataName;
        szAtributeDataName = SECTION_ATTRIBUTE_DATA;
        szAtributeDataName.append(":");
        szAtributeDataName.append(pszBandSectionValue);
        VSIFree(pszBandSectionValue);

        char *pszValue = MMReturnValueFromSectionINIFile(
            pszRELFileName, szAtributeDataName, KEY_NomFitxer);

        CPLString osRawBandFileName = pszValue ? pszValue : "";
        VSIFree(pszValue);

        if (osRawBandFileName.empty())
        {
            CPLString osBandFileName =
                MMRGetFileNameFromRelName(pszRELFileName);
            if (osBandFileName.empty())
            {
                VSIFree(pszFieldNames);
                return CE_Failure;
            }
        }
        else
        {
            if (!EQUAL(osRawBandFileName, pszIMGFile))
            {
                continue;
            }
            break;  // Found
        }
    }

    CSLDestroy(papszTokens);
    VSIFree(pszFieldNames);

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
        return FALSE;

    CPLString osRELAndBandName = osFilename.substr(osMMRPrefix.size());

    char **papszTokens = CSLTokenizeString2(osRELAndBandName, ",", 0);
    const int nTokens = CSLCount(papszTokens);
    // Getting the REL associated to the bands
    // We need the REL and at least one band (index + name).
    if (nTokens < 2)
        return FALSE;

    // Let's remove "\"" if existant.
    CPLString osRELName = papszTokens[0];
    osRELName.erase(std::remove(osRELName.begin(), osRELName.end(), '\"'),
                    osRELName.end());

    if (MMCheck_REL_FILE(osRELName))
        return FALSE;

    // Getting the index + internal names of the bands
    for (int nIBand = 1; nIBand < nTokens; nIBand++)
    {
        // Let's check that this band (papszTokens[nIBand]) is in the REL file.
        CPLString osBandName = papszTokens[nIBand];
        // Let's remove "\"" if existant.
        osBandName.erase(
            std::remove(osBandName.begin(), osBandName.end(), '\"'),
            osBandName.end());
        if (CE_None != CheckBandInRel(osRELName, osBandName))
        {
            CSLDestroy(papszTokens);
            return FALSE;
        }
    }
    CSLDestroy(papszTokens);
    return TRUE;
}

/************************************************************************/
/*                     IdentifyFile()                                */
/************************************************************************/
int MMRRel::IdentifyFile(CPLString pszFileName)
{
    // Verify that this is a MiraMon IMG or REL file.
    // If IMG, a sidecar file I.rel with reference to
    // poOpenInfo->pszFilename must exist
    CPLString pszRELFile =
        GetAssociatedMetadataFileName((const char *)pszFileName);

    if (EQUAL(pszRELFile, ""))
        return FALSE;

    // Some versions of REL files are not allowed.
    if (MMCheck_REL_FILE(pszRELFile))
        return FALSE;

    return TRUE;
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
char *MMRRel::GetMetadataValue(const char *pszMainSection,
                               const char *pszSubSection,
                               const char *pszSubSubSection, const char *pszKey)
{
    // Searches in [pszMainSection:pszSubSection]
    CPLString szAtributeDataName;
    szAtributeDataName = pszMainSection;
    szAtributeDataName.append(":");
    szAtributeDataName.append(pszSubSection);
    szAtributeDataName.append(":");
    szAtributeDataName.append(pszSubSubSection);

    char *pszValue = MMReturnValueFromSectionINIFile(
        GetRELNameChar(), szAtributeDataName, pszKey);
    if (pszValue)
        return pszValue;

    // If the value is not found then searches in [pszMainSection]
    return MMReturnValueFromSectionINIFile(GetRELNameChar(), pszSubSubSection,
                                           pszKey);
}

char *MMRRel::GetMetadataValue(const char *pszMainSection,
                               const char *pszSubSection, const char *pszKey)
{
    // Searches in [pszMainSection:pszSubSection]
    CPLString szAtributeDataName;
    szAtributeDataName = pszMainSection;
    szAtributeDataName.append(":");
    szAtributeDataName.append(pszSubSection);

    char *pszValue = MMReturnValueFromSectionINIFile(
        GetRELNameChar(), szAtributeDataName, pszKey);
    if (pszValue)
        return pszValue;

    // If the value is not found then searches in [pszMainSection]
    return MMReturnValueFromSectionINIFile(GetRELNameChar(), pszMainSection,
                                           pszKey);
}

char *MMRRel::GetMetadataValue(const char *pszSection, const char *pszKey)
{
    return MMReturnValueFromSectionINIFile(GetRELNameChar(), pszSection,
                                           pszKey);
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
CPLErr MMRRel::ParseBandInfo(MMRInfo_t *psInfo)

{
    if (!psInfo || !psInfo->fRel)
        return CE_Fatal;

    psInfo->nBands = 0;

    CPLString pszRELFileName = psInfo->osRELFileName;

    char *pszFieldNames = psInfo->fRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA,
                                                         Key_IndexsNomsCamps);

    if (!pszFieldNames)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, "%s-%s section-key \
            should exist in %s.",
                 SECTION_ATTRIBUTE_DATA, Key_IndexsNomsCamps,
                 pszRELFileName.c_str());
        return CE_Failure;
    }

    // Separator ,
    char **papszTokens = CSLTokenizeString2(pszFieldNames, ",", 0);
    const int nTokenCount = CSLCount(papszTokens);

    if (!nTokenCount)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, "No bands in file \
            %s.",
                 pszRELFileName.c_str());
        VSIFree(pszFieldNames);
        return CE_Failure;
    }

    CPLString pszBandSectionKey;
    CPLString pszFileAux;
    char *pszBandSectionValue;
    for (int i = 0; i < nTokenCount; i++)
    {
        pszBandSectionKey = KEY_NomCamp;
        pszBandSectionKey.append("_");
        pszBandSectionKey.append(papszTokens[i]);

        pszBandSectionValue = psInfo->fRel->GetMetadataValue(
            SECTION_ATTRIBUTE_DATA, pszBandSectionKey);

        if (!pszBandSectionValue)
            continue;

        MM_RemoveWhitespacesFromEndOfString(pszBandSectionValue);

        psInfo->papoBand = static_cast<MMRBand **>(
            CPLRealloc(psInfo->papoBand,
                       sizeof(MMRBand *) * (psInfo->nBands + (size_t)1)));
        psInfo->papoBand[psInfo->nBands] =
            new MMRBand(psInfo, pszBandSectionValue);

        VSIFree(pszBandSectionValue);
        if (!psInfo->bBandInTheList)
        {
            delete psInfo->papoBand[psInfo->nBands];
            continue;
        }
        if (psInfo->papoBand[psInfo->nBands]->nWidth == 0)
        {
            CSLDestroy(papszTokens);
            VSIFree(pszFieldNames);
            delete psInfo->papoBand[psInfo->nBands];
            return CE_Failure;
        }

        psInfo->papoBand[psInfo->nBands]->osRELFileName = pszRELFileName;
        psInfo->nBands++;
    }

    CSLDestroy(papszTokens);
    VSIFree(pszFieldNames);

    return CE_None;
}
