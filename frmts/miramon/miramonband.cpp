/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements MMRBand class.
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

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "miramon.h"
#include "gdal_priv.h"

/************************************************************************/
/*                              MMRBand()                               */
/************************************************************************/

MMRBand::MMRBand(MMRInfo_t *psInfoIn, const char *pszSection)
    : nBlocks(0), panBlockStart(nullptr), panBlockSize(nullptr),
      panBlockFlag(nullptr), nBlockStart(0), nBlockSize(0), nLayerStackCount(0),
      nLayerStackIndex(0), nPCTColors(-1), padfPCTBins(nullptr),
      psInfo(psInfoIn), fpExternal(nullptr),
      eDataType(static_cast<EPTType>(EPT_MIN)), poNode(nullptr), nBlockXSize(1),
      nBlockYSize(1), nWidth(psInfoIn->nXSize), nHeight(psInfo->nYSize),
      nBlocksPerRow(1), nBlocksPerColumn(1), bNoDataSet(false), dfNoData(0.0)
{
    char *pszRELFilename = psInfo->pszRELFilename;

    // Band pixels are in a separated file
    const char *pszRelativeBandFileName = MMReturnValueFromSectionINIFile(
        pszRELFilename, pszSection, KEY_NomFitxer);
    const CPLString oPath = CPLGetPathSafe(pszRELFilename);
    pszBandFileName = CPLFormFilenameSafe(oPath, pszRelativeBandFileName, "");

    char *pszCompType = MMReturnValueFromSectionINIFile(
        pszRELFilename, pszSection, "TipusCompressio");
    if (pszCompType)
    {
        if (MMGetDataTypeAndBytesPerPixel(pszCompType,
                                          &psInfo->nCompressionType,
                                          &psInfo->nBytesPerPixel) == 1)
        {
            VSIFree(pszCompType);
            nWidth = 0;
            nHeight = 0;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "MMRBand::MMRBand : nDataType=%s unhandled", pszCompType);
            return;
        }
        VSIFree(pszCompType);
    }

    apadfPCT[0] = nullptr;
    apadfPCT[1] = nullptr;
    apadfPCT[2] = nullptr;
    apadfPCT[3] = nullptr;

    // Number of rows/columns is specific or gets the dataset value
    char *pszColumns =
        MMReturnValueFromSectionINIFile(pszRELFilename, pszSection, "columns");
    if (pszColumns)
        psInfo->nXSize = atoi(pszColumns);
    VSIFree(pszColumns);

    char *pszRows =
        MMReturnValueFromSectionINIFile(pszRELFilename, pszSection, "rows");
    if (pszRows)
        psInfo->nYSize = atoi(pszRows);
    VSIFree(pszRows);

    if (nWidth <= 0 || nHeight <= 0)
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "MMRBand::MMRBand : (nWidth <= 0 || nHeight <= 0)");
        return;
    }

    // Check for nodata.  This is really an RDO (ESRI Raster Data Objects?),
    // not used by Imagine itself.
}

/************************************************************************/
/*                              ~MMRBand()                              */
/************************************************************************/

MMRBand::~MMRBand()

{
    CPLFree(panBlockStart);
    CPLFree(panBlockSize);
    CPLFree(panBlockFlag);

    CPLFree(apadfPCT[0]);
    CPLFree(apadfPCT[1]);
    CPLFree(apadfPCT[2]);
    CPLFree(apadfPCT[3]);
    CPLFree(padfPCTBins);

    if (fpExternal != nullptr)
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpExternal));
}

/************************************************************************/
/*                           LoadBlockInfo()                            */
/************************************************************************/

CPLErr MMRBand::LoadBlockInfo()

{
    if (panBlockFlag != nullptr)
        return CE_None;

    MMREntry *poDMS = poNode->GetNamedChild("RasterDMS");
    if (poDMS == nullptr)
    {
        if (poNode->GetNamedChild("ExternalRasterDMS") != nullptr)
            return LoadExternalBlockInfo();

        CPLError(CE_Failure, CPLE_AppDefined,
                 "Can't find RasterDMS field in Eimg_Layer with block list.");

        return CE_Failure;
    }

    if (sizeof(vsi_l_offset) + 2 * sizeof(int) >
        (~(size_t)0) / static_cast<unsigned int>(nBlocks))
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Too many blocks");
        return CE_Failure;
    }
    const int MAX_INITIAL_BLOCKS = 1000 * 1000;
    const int nInitBlocks = std::min(nBlocks, MAX_INITIAL_BLOCKS);
    panBlockStart = static_cast<vsi_l_offset *>(
        VSI_MALLOC2_VERBOSE(sizeof(vsi_l_offset), nInitBlocks));
    panBlockSize =
        static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nInitBlocks));
    panBlockFlag =
        static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nInitBlocks));

    if (panBlockStart == nullptr || panBlockSize == nullptr ||
        panBlockFlag == nullptr)
    {
        CPLFree(panBlockStart);
        CPLFree(panBlockSize);
        CPLFree(panBlockFlag);
        panBlockStart = nullptr;
        panBlockSize = nullptr;
        panBlockFlag = nullptr;
        return CE_Failure;
    }

    for (int iBlock = 0; iBlock < nBlocks; iBlock++)
    {
        CPLErr eErr = CE_None;

        if (iBlock == MAX_INITIAL_BLOCKS)
        {
            vsi_l_offset *panBlockStartNew =
                static_cast<vsi_l_offset *>(VSI_REALLOC_VERBOSE(
                    panBlockStart, sizeof(vsi_l_offset) * nBlocks));
            if (panBlockStartNew == nullptr)
            {
                CPLFree(panBlockStart);
                CPLFree(panBlockSize);
                CPLFree(panBlockFlag);
                panBlockStart = nullptr;
                panBlockSize = nullptr;
                panBlockFlag = nullptr;
                return CE_Failure;
            }
            panBlockStart = panBlockStartNew;

            int *panBlockSizeNew = static_cast<int *>(
                VSI_REALLOC_VERBOSE(panBlockSize, sizeof(int) * nBlocks));
            if (panBlockSizeNew == nullptr)
            {
                CPLFree(panBlockStart);
                CPLFree(panBlockSize);
                CPLFree(panBlockFlag);
                panBlockStart = nullptr;
                panBlockSize = nullptr;
                panBlockFlag = nullptr;
                return CE_Failure;
            }
            panBlockSize = panBlockSizeNew;

            int *panBlockFlagNew = static_cast<int *>(
                VSI_REALLOC_VERBOSE(panBlockFlag, sizeof(int) * nBlocks));
            if (panBlockFlagNew == nullptr)
            {
                CPLFree(panBlockStart);
                CPLFree(panBlockSize);
                CPLFree(panBlockFlag);
                panBlockStart = nullptr;
                panBlockSize = nullptr;
                panBlockFlag = nullptr;
                return CE_Failure;
            }
            panBlockFlag = panBlockFlagNew;
        }

        char szVarName[64] = {};
        snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].offset", iBlock);
        panBlockStart[iBlock] =
            static_cast<GUInt32>(poDMS->GetIntField(szVarName, &eErr));
        if (eErr == CE_Failure)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read %s", szVarName);
            return eErr;
        }

        snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].size", iBlock);
        panBlockSize[iBlock] = poDMS->GetIntField(szVarName, &eErr);
        if (eErr == CE_Failure)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read %s", szVarName);
            return eErr;
        }
        if (panBlockSize[iBlock] < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid block size");
            return CE_Failure;
        }

        snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].logvalid",
                 iBlock);
        const int nLogvalid = poDMS->GetIntField(szVarName, &eErr);
        if (eErr == CE_Failure)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read %s", szVarName);
            return eErr;
        }

        snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].compressionType",
                 iBlock);
        const int nCompressType = poDMS->GetIntField(szVarName, &eErr);
        if (eErr == CE_Failure)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read %s", szVarName);
            return eErr;
        }

        panBlockFlag[iBlock] = 0;
        if (nLogvalid)
            panBlockFlag[iBlock] |= BFLG_VALID;
        if (nCompressType != 0)
            panBlockFlag[iBlock] |= BFLG_COMPRESSED;
    }

    return CE_None;
}

/************************************************************************/
/*                       LoadExternalBlockInfo()                        */
/************************************************************************/

CPLErr MMRBand::LoadExternalBlockInfo()

{
    if (panBlockFlag != nullptr)
        return CE_None;

    // Get the info structure.
    MMREntry *poDMS = poNode->GetNamedChild("ExternalRasterDMS");
    CPLAssert(poDMS != nullptr);

    nLayerStackCount = poDMS->GetIntField("layerStackCount");
    nLayerStackIndex = poDMS->GetIntField("layerStackIndex");

    // Open raw data file.
    /*const std::string osFullFilename = MMRGetIGEFilename(psInfo);
    if (osFullFilename.empty())
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Cannot find external data file name");
        return CE_Failure;
    }

    if (psInfo->eAccess == MMR_ReadOnly)
        fpExternal = VSIFOpenL(osFullFilename.c_str(), "rb");
    else
        fpExternal = VSIFOpenL(osFullFilename.c_str(), "r+b");
    if (fpExternal == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Unable to open external data file: %s",
                 osFullFilename.c_str());
        return CE_Failure;
    }

    // Verify header.
    char szHeader[49] = {};

    if (VSIFReadL(szHeader, sizeof(szHeader), 1, fpExternal) != 1 ||
        !STARTS_WITH(szHeader, "ERDAS_IMG_EXTERNAL_RASTER"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Raw data file %s appears to be corrupt.",
                 osFullFilename.c_str());
        return CE_Failure;
    }

    // Allocate blockmap.
    panBlockFlag =
        static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nBlocks));
    if (panBlockFlag == nullptr)
    {
        return CE_Failure;
    }

    // Load the validity bitmap.
    const int nBytesPerRow = (nBlocksPerRow + 7) / 8;
    unsigned char *pabyBlockMap = static_cast<unsigned char *>(
        VSI_MALLOC_VERBOSE(nBytesPerRow * nBlocksPerColumn + 20));
    if (pabyBlockMap == nullptr)
    {
        return CE_Failure;
    }

    if (VSIFSeekL(fpExternal,
                  poDMS->GetBigIntField("layerStackValidFlagsOffset"),
                  SEEK_SET) < 0 ||
        VSIFReadL(pabyBlockMap, nBytesPerRow * nBlocksPerColumn + 20, 1,
                  fpExternal) != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Failed to read block validity map.");
        return CE_Failure;
    }

    // Establish block information.  Block position is computed
    // from data base address.  Blocks are never compressed.
    // Validity is determined from the validity bitmap.

    nBlockStart = poDMS->GetBigIntField("layerStackDataOffset");
    nBlockSize = (nBlockXSize * static_cast<vsi_l_offset>(nBlockYSize) *
                      MMRGetDataTypeBits(eDataType) +
                  7) /
                 8;

    for (int iBlock = 0; iBlock < nBlocks; iBlock++)
    {
        const int nColumn = iBlock % nBlocksPerRow;
        const int nRow = iBlock / nBlocksPerRow;
        const int nBit = nRow * nBytesPerRow * 8 + nColumn + 20 * 8;

        if ((pabyBlockMap[nBit >> 3] >> (nBit & 7)) & 0x1)
            panBlockFlag[iBlock] = BFLG_VALID;
        else
            panBlockFlag[iBlock] = 0;
    }

    CPLFree(pabyBlockMap);
    */

    return CE_None;
}

/************************************************************************/
/*                          UncompressBlock()                           */
/*                                                                      */
/*      Uncompress ESRI Grid compression format block.                  */
/************************************************************************/

// TODO(schwehr): Get rid of this macro without a goto.
#define CHECK_ENOUGH_BYTES(n)                                                  \
    if (nSrcBytes < (n))                                                       \
    {                                                                          \
        CPLError(CE_Failure, CPLE_AppDefined,                                  \
                 "Not enough bytes in compressed block");                      \
        return CE_Failure;                                                     \
    }

static CPLErr UncompressBlock(GByte *pabyCData, int nSrcBytes, GByte *pabyDest,
                              int nMaxPixels, EPTType eDataType)

{
    CHECK_ENOUGH_BYTES(13);

    const GUInt32 nDataMin = CPL_LSBUINT32PTR(pabyCData);
    const GInt32 nNumRuns = CPL_LSBSINT32PTR(pabyCData + 4);
    const GInt32 nDataOffset = CPL_LSBSINT32PTR(pabyCData + 8);

    const int nNumBits = pabyCData[12];

    // If this is not run length encoded, but just reduced
    // precision, handle it now.

    int nPixelsOutput = 0;
    GByte *pabyValues = nullptr;
    int nValueBitOffset = 0;

    if (nNumRuns == -1)
    {
        pabyValues = pabyCData + 13;
        nValueBitOffset = 0;

        if (nNumBits > INT_MAX / nMaxPixels ||
            nNumBits * nMaxPixels > INT_MAX - 7 ||
            (nNumBits * nMaxPixels + 7) / 8 > INT_MAX - 13)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Integer overflow : nNumBits * nMaxPixels + 7");
            return CE_Failure;
        }
        CHECK_ENOUGH_BYTES(13 + (nNumBits * nMaxPixels + 7) / 8);

        // Loop over block pixels.
        for (nPixelsOutput = 0; nPixelsOutput < nMaxPixels; nPixelsOutput++)
        {
            // Extract the data value in a way that depends on the number
            // of bits in it.

            int nRawValue = 0;

            if (nNumBits == 0)
            {
                // nRawValue = 0;
            }
            else if (nNumBits == 1)
            {
                nRawValue = (pabyValues[nValueBitOffset >> 3] >>
                             (nValueBitOffset & 7)) &
                            0x1;
                nValueBitOffset++;
            }
            else if (nNumBits == 2)
            {
                nRawValue = (pabyValues[nValueBitOffset >> 3] >>
                             (nValueBitOffset & 7)) &
                            0x3;
                nValueBitOffset += 2;
            }
            else if (nNumBits == 4)
            {
                nRawValue = (pabyValues[nValueBitOffset >> 3] >>
                             (nValueBitOffset & 7)) &
                            0xf;
                nValueBitOffset += 4;
            }
            else if (nNumBits == 8)
            {
                nRawValue = *pabyValues;
                pabyValues++;
            }
            else if (nNumBits == 16)
            {
                nRawValue = 256 * *(pabyValues++);
                nRawValue += *(pabyValues++);
            }
            else if (nNumBits == 32)
            {
                memcpy(&nRawValue, pabyValues, 4);
                CPL_MSBPTR32(&nRawValue);
                pabyValues += 4;
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported nNumBits value: %d", nNumBits);
                return CE_Failure;
            }

            // Offset by the minimum value.
            const int nDataValue = CPLUnsanitizedAdd<int>(nRawValue, nDataMin);

            // Now apply to the output buffer in a type specific way.
            if (eDataType == EPT_u8)
            {
                ((GByte *)pabyDest)[nPixelsOutput] =
                    static_cast<GByte>(nDataValue);
            }
            else if (eDataType == EPT_u1)
            {
                if (nDataValue == 1)
                    pabyDest[nPixelsOutput >> 3] |=
                        (1 << (nPixelsOutput & 0x7));
                else
                    pabyDest[nPixelsOutput >> 3] &=
                        ~(1 << (nPixelsOutput & 0x7));
            }
            else if (eDataType == EPT_u2)
            {
                // nDataValue & 0x3 is just to avoid UBSAN warning on shifting
                // negative values
                if ((nPixelsOutput & 0x3) == 0)
                    pabyDest[nPixelsOutput >> 2] =
                        static_cast<GByte>(nDataValue);
                else if ((nPixelsOutput & 0x3) == 1)
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 2);
                else if ((nPixelsOutput & 0x3) == 2)
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 4);
                else
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 6);
            }
            else if (eDataType == EPT_u4)
            {
                // nDataValue & 0xF is just to avoid UBSAN warning on shifting
                // negative values
                if ((nPixelsOutput & 0x1) == 0)
                    pabyDest[nPixelsOutput >> 1] =
                        static_cast<GByte>(nDataValue);
                else
                    pabyDest[nPixelsOutput >> 1] |=
                        static_cast<GByte>((nDataValue & 0xF) << 4);
            }
            else if (eDataType == EPT_s8)
            {
                ((GInt8 *)pabyDest)[nPixelsOutput] =
                    static_cast<GInt8>(nDataValue);
            }
            else if (eDataType == EPT_u16)
            {
                ((GUInt16 *)pabyDest)[nPixelsOutput] =
                    static_cast<GUInt16>(nDataValue);
            }
            else if (eDataType == EPT_s16)
            {
                ((GInt16 *)pabyDest)[nPixelsOutput] =
                    static_cast<GInt16>(nDataValue);
            }
            else if (eDataType == EPT_s32)
            {
                ((GInt32 *)pabyDest)[nPixelsOutput] = nDataValue;
            }
            else if (eDataType == EPT_u32)
            {
                ((GUInt32 *)pabyDest)[nPixelsOutput] = nDataValue;
            }
            else if (eDataType == EPT_f32)
            {
                // Note, floating point values are handled as if they were
                // signed 32-bit integers (bug #1000).
                memcpy(&(((float *)pabyDest)[nPixelsOutput]), &nDataValue,
                       sizeof(float));
            }
            else
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Attempt to uncompress an unsupported pixel data type.");
                return CE_Failure;
            }
        }

        return CE_None;
    }

    // Establish data pointers for runs.
    if (nNumRuns < 0 || nDataOffset < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "nNumRuns=%d, nDataOffset=%d",
                 nNumRuns, nDataOffset);
        return CE_Failure;
    }

    if (nNumRuns != 0 &&
        (nNumBits > INT_MAX / nNumRuns || nNumBits * nNumRuns > INT_MAX - 7 ||
         (nNumBits * nNumRuns + 7) / 8 > INT_MAX - nDataOffset))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Integer overflow: nDataOffset + (nNumBits * nNumRuns + 7)/8");
        return CE_Failure;
    }
    CHECK_ENOUGH_BYTES(nDataOffset + (nNumBits * nNumRuns + 7) / 8);

    GByte *pabyCounter = pabyCData + 13;
    int nCounterOffset = 13;
    pabyValues = pabyCData + nDataOffset;
    nValueBitOffset = 0;

    // Loop over runs.
    for (int iRun = 0; iRun < nNumRuns; iRun++)
    {
        int nRepeatCount = 0;

        // Get the repeat count.  This can be stored as one, two, three
        // or four bytes depending on the low order two bits of the
        // first byte.
        CHECK_ENOUGH_BYTES(nCounterOffset + 1);
        if ((*pabyCounter & 0xc0) == 0x00)
        {
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nCounterOffset++;
        }
        else if (((*pabyCounter) & 0xc0) == 0x40)
        {
            CHECK_ENOUGH_BYTES(nCounterOffset + 2);
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nCounterOffset += 2;
        }
        else if (((*pabyCounter) & 0xc0) == 0x80)
        {
            CHECK_ENOUGH_BYTES(nCounterOffset + 3);
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nCounterOffset += 3;
        }
        else if (((*pabyCounter) & 0xc0) == 0xc0)
        {
            CHECK_ENOUGH_BYTES(nCounterOffset + 4);
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nCounterOffset += 4;
        }

        // Extract the data value in a way that depends on the number
        // of bits in it.
        int nDataValue = 0;

        if (nNumBits == 0)
        {
            // nDataValue = 0;
        }
        else if (nNumBits == 1)
        {
            nDataValue =
                (pabyValues[nValueBitOffset >> 3] >> (nValueBitOffset & 7)) &
                0x1;
            nValueBitOffset++;
        }
        else if (nNumBits == 2)
        {
            nDataValue =
                (pabyValues[nValueBitOffset >> 3] >> (nValueBitOffset & 7)) &
                0x3;
            nValueBitOffset += 2;
        }
        else if (nNumBits == 4)
        {
            nDataValue =
                (pabyValues[nValueBitOffset >> 3] >> (nValueBitOffset & 7)) &
                0xf;
            nValueBitOffset += 4;
        }
        else if (nNumBits == 8)
        {
            nDataValue = *pabyValues;
            pabyValues++;
        }
        else if (nNumBits == 16)
        {
            nDataValue = 256 * *(pabyValues++);
            nDataValue += *(pabyValues++);
        }
        else if (nNumBits == 32)
        {
            memcpy(&nDataValue, pabyValues, 4);
            CPL_MSBPTR32(&nDataValue);
            pabyValues += 4;
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported, "nNumBits = %d", nNumBits);
            return CE_Failure;
        }

        // Offset by the minimum value.
        nDataValue = CPLUnsanitizedAdd<int>(nDataValue, nDataMin);

        // Now apply to the output buffer in a type specific way.
        if (nRepeatCount > INT_MAX - nPixelsOutput ||
            nPixelsOutput + nRepeatCount > nMaxPixels)
        {
            CPLDebug("MiraMonRaster", "Repeat count too big: %d", nRepeatCount);
            nRepeatCount = nMaxPixels - nPixelsOutput;
        }

        if (eDataType == EPT_u8)
        {
            for (int i = 0; i < nRepeatCount; i++)
            {
#if DEBUG_VERBOSE
                // TODO(schwehr): Do something smarter with out-of-range data.
                // Bad data can trigger this assert.  r23498
                CPLAssert(nDataValue < 256);
#endif
                ((GByte *)pabyDest)[nPixelsOutput++] =
                    static_cast<GByte>(nDataValue);
            }
        }
        else if (eDataType == EPT_u16)
        {
            for (int i = 0; i < nRepeatCount; i++)
            {
#if DEBUG_VERBOSE
                CPLAssert(nDataValue >= 0);
                CPLAssert(nDataValue < 65536);
#endif
                ((GUInt16 *)pabyDest)[nPixelsOutput++] =
                    static_cast<GUInt16>(nDataValue);
            }
        }
        else if (eDataType == EPT_s8)
        {
            for (int i = 0; i < nRepeatCount; i++)
            {
#if DEBUG_VERBOSE
                // TODO(schwehr): Do something smarter with out-of-range data.
                // Bad data can trigger this assert.  r23498
                CPLAssert(nDataValue >= -127);
                CPLAssert(nDataValue < 128);
#endif
                ((GByte *)pabyDest)[nPixelsOutput++] =
                    static_cast<GByte>(nDataValue);
            }
        }
        else if (eDataType == EPT_s16)
        {
            for (int i = 0; i < nRepeatCount; i++)
            {
#if DEBUG_VERBOSE
                // TODO(schwehr): Do something smarter with out-of-range data.
                // Bad data can trigger this assert.  r23498
                CPLAssert(nDataValue >= -32768);
                CPLAssert(nDataValue < 32768);
#endif
                ((GInt16 *)pabyDest)[nPixelsOutput++] =
                    static_cast<GInt16>(nDataValue);
            }
        }
        else if (eDataType == EPT_u32)
        {
            for (int i = 0; i < nRepeatCount; i++)
            {
#if DEBUG_VERBOSE
                // TODO(schwehr): Do something smarter with out-of-range data.
                // Bad data can trigger this assert.  r23498
                CPLAssert(nDataValue >= 0);
#endif
                ((GUInt32 *)pabyDest)[nPixelsOutput++] =
                    static_cast<GUInt32>(nDataValue);
            }
        }
        else if (eDataType == EPT_s32)
        {
            for (int i = 0; i < nRepeatCount; i++)
            {
                ((GInt32 *)pabyDest)[nPixelsOutput++] =
                    static_cast<GInt32>(nDataValue);
            }
        }
        else if (eDataType == EPT_f32)
        {
            float fDataValue = 0.0f;

            memcpy(&fDataValue, &nDataValue, 4);
            for (int i = 0; i < nRepeatCount; i++)
            {
                ((float *)pabyDest)[nPixelsOutput++] = fDataValue;
            }
        }
        else if (eDataType == EPT_u1)
        {
#ifdef DEBUG_VERBOSE
            CPLAssert(nDataValue == 0 || nDataValue == 1);
#endif
            if (nDataValue == 1)
            {
                for (int i = 0; i < nRepeatCount; i++)
                {
                    pabyDest[nPixelsOutput >> 3] |=
                        (1 << (nPixelsOutput & 0x7));
                    nPixelsOutput++;
                }
            }
            else
            {
                for (int i = 0; i < nRepeatCount; i++)
                {
                    pabyDest[nPixelsOutput >> 3] &=
                        ~(1 << (nPixelsOutput & 0x7));
                    nPixelsOutput++;
                }
            }
        }
        else if (eDataType == EPT_u2)
        {
#ifdef DEBUG_VERBOSE
            CPLAssert(nDataValue >= 0 && nDataValue < 4);
#endif
            for (int i = 0; i < nRepeatCount; i++)
            {
                if ((nPixelsOutput & 0x3) == 0)
                    pabyDest[nPixelsOutput >> 2] =
                        static_cast<GByte>(nDataValue);
                else if ((nPixelsOutput & 0x3) == 1)
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 2);
                else if ((nPixelsOutput & 0x3) == 2)
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 4);
                else
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 6);
                nPixelsOutput++;
            }
        }
        else if (eDataType == EPT_u4)
        {
#ifdef DEBUG_VERBOSE
            CPLAssert(nDataValue >= 0 && nDataValue < 16);
#endif
            for (int i = 0; i < nRepeatCount; i++)
            {
                if ((nPixelsOutput & 0x1) == 0)
                    pabyDest[nPixelsOutput >> 1] =
                        static_cast<GByte>(nDataValue);
                else
                    pabyDest[nPixelsOutput >> 1] |=
                        static_cast<GByte>((nDataValue & 0xF) << 4);

                nPixelsOutput++;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attempt to uncompress an unsupported pixel data type.");
            return CE_Failure;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             NullBlock()                              */
/*                                                                      */
/*      Set the block buffer to zero or the nodata value as             */
/*      appropriate.                                                    */
/************************************************************************/

void MMRBand::NullBlock(void *pData)

{
    const int nChunkSize = std::max(1, MMRGetDataTypeBits(eDataType) / 8);
    int nWords = nBlockXSize * nBlockYSize;

    if (!bNoDataSet)
    {
#ifdef ESRI_BUILD
        // We want special defaulting for 1 bit data in ArcGIS.
        if (eDataType >= EPT_u2)
            memset(pData, 0, static_cast<size_t>(nChunkSize) * nWords);
        else
            memset(pData, 255, static_cast<size_t>(nChunkSize) * nWords);
#else
        memset(pData, 0, static_cast<size_t>(nChunkSize) * nWords);
#endif
    }
    else
    {
        GByte abyTmp[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        switch (eDataType)
        {
            case EPT_u1:
            {
                nWords = (nWords + 7) / 8;
                if (dfNoData != 0.0)
                    ((unsigned char *)abyTmp)[0] = 0xff;
                else
                    ((unsigned char *)abyTmp)[0] = 0x00;
            }
            break;

            case EPT_u2:
            {
                nWords = (nWords + 3) / 4;
                if (dfNoData == 0.0)
                    ((unsigned char *)abyTmp)[0] = 0x00;
                else if (dfNoData == 1.0)
                    ((unsigned char *)abyTmp)[0] = 0x55;
                else if (dfNoData == 2.0)
                    ((unsigned char *)abyTmp)[0] = 0xaa;
                else
                    ((unsigned char *)abyTmp)[0] = 0xff;
            }
            break;

            case EPT_u4:
            {
                const unsigned char byVal = static_cast<unsigned char>(
                    std::max(0, std::min(15, static_cast<int>(dfNoData))));

                nWords = (nWords + 1) / 2;

                ((unsigned char *)abyTmp)[0] = byVal + (byVal << 4);
            }
            break;

            case EPT_u8:
                ((unsigned char *)abyTmp)[0] = static_cast<unsigned char>(
                    std::max(0, std::min(255, static_cast<int>(dfNoData))));
                break;

            case EPT_s8:
                ((signed char *)abyTmp)[0] = static_cast<signed char>(
                    std::max(-128, std::min(127, static_cast<int>(dfNoData))));
                break;

            case EPT_u16:
            {
                GUInt16 nTmp = static_cast<GUInt16>(dfNoData);
                memcpy(abyTmp, &nTmp, sizeof(nTmp));
                break;
            }

            case EPT_s16:
            {
                GInt16 nTmp = static_cast<GInt16>(dfNoData);
                memcpy(abyTmp, &nTmp, sizeof(nTmp));
                break;
            }

            case EPT_u32:
            {
                GUInt32 nTmp = static_cast<GUInt32>(dfNoData);
                memcpy(abyTmp, &nTmp, sizeof(nTmp));
                break;
            }

            case EPT_s32:
            {
                GInt32 nTmp = static_cast<GInt32>(dfNoData);
                memcpy(abyTmp, &nTmp, sizeof(nTmp));
                break;
            }

            case EPT_f32:
            {
                float fTmp = static_cast<float>(dfNoData);
                memcpy(abyTmp, &fTmp, sizeof(fTmp));
                break;
            }

            case EPT_f64:
            {
                memcpy(abyTmp, &dfNoData, sizeof(dfNoData));
                break;
            }

            case EPT_c64:
            {
                float fTmp = static_cast<float>(dfNoData);
                memcpy(abyTmp, &fTmp, sizeof(fTmp));
                memset(abyTmp + 4, 0, sizeof(float));
                break;
            }

            case EPT_c128:
            {
                memcpy(abyTmp, &dfNoData, sizeof(dfNoData));
                memset(abyTmp + 8, 0, sizeof(double));
                break;
            }
        }

        for (int i = 0; i < nWords; i++)
            memcpy(((GByte *)pData) + nChunkSize * i, abyTmp, nChunkSize);
    }
}

/************************************************************************/
/*                           GetRasterBlock()                           */
/************************************************************************/

CPLErr MMRBand::GetRasterBlock(int nXBlock, int nYBlock, void *pData,
                               int nDataSize)

{
    if (LoadBlockInfo() != CE_None)
        return CE_Failure;

    const int iBlock = nXBlock + nYBlock * nBlocksPerRow;
    const int nDataTypeSizeBytes =
        std::max(1, MMRGetDataTypeBits(eDataType) / 8);
    const int nGDALBlockSize = nDataTypeSizeBytes * nBlockXSize * nBlockYSize;

    // If the block isn't valid, we just return all zeros, and an
    // indication of success.
    if ((panBlockFlag[iBlock] & BFLG_VALID) == 0)
    {
        NullBlock(pData);
        return CE_None;
    }

    // Otherwise we really read the data.
    vsi_l_offset nBlockOffset = 0;
    VSILFILE *fpData = nullptr;

    // Calculate block offset in case we have spill file. Use predefined
    // block map otherwise.
    if (fpExternal)
    {
        fpData = fpExternal;
        nBlockOffset = nBlockStart + nBlockSize * iBlock * nLayerStackCount +
                       nLayerStackIndex * nBlockSize;
    }
    else
    {
        fpData = psInfo->fp;
        nBlockOffset = panBlockStart[iBlock];
        nBlockSize = panBlockSize[iBlock];
    }

    if (VSIFSeekL(fpData, nBlockOffset, SEEK_SET) != 0)
    {
        // XXX: We will not report error here, because file just may be
        // in update state and data for this block will be available later.
        if (psInfo->eAccess == MMR_Update)
        {
            memset(pData, 0, nGDALBlockSize);
            return CE_None;
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Seek to %x:%08x on %p failed\n%s",
                     static_cast<int>(nBlockOffset >> 32),
                     static_cast<int>(nBlockOffset & 0xffffffff), fpData,
                     VSIStrerror(errno));
            return CE_Failure;
        }
    }

    // If the block is compressed, read into an intermediate buffer
    // and convert.
    if (panBlockFlag[iBlock] & BFLG_COMPRESSED)
    {
        GByte *pabyCData = static_cast<GByte *>(
            VSI_MALLOC_VERBOSE(static_cast<size_t>(nBlockSize)));
        if (pabyCData == nullptr)
        {
            return CE_Failure;
        }

        if (VSIFReadL(pabyCData, static_cast<size_t>(nBlockSize), 1, fpData) !=
            1)
        {
            CPLFree(pabyCData);

            // XXX: Suppose that file in update state
            if (psInfo->eAccess == MMR_Update)
            {
                memset(pData, 0, nGDALBlockSize);
                return CE_None;
            }
            else
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Read of %d bytes at %x:%08x on %p failed.\n%s",
                         static_cast<int>(nBlockSize),
                         static_cast<int>(nBlockOffset >> 32),
                         static_cast<int>(nBlockOffset & 0xffffffff), fpData,
                         VSIStrerror(errno));
                return CE_Failure;
            }
        }

        CPLErr eErr = UncompressBlock(pabyCData, static_cast<int>(nBlockSize),
                                      static_cast<GByte *>(pData),
                                      nBlockXSize * nBlockYSize, eDataType);

        CPLFree(pabyCData);

        return eErr;
    }

    // Read uncompressed data directly into the return buffer.
    if (nDataSize != -1 &&
        (nBlockSize > INT_MAX || static_cast<int>(nBlockSize) > nDataSize))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid block size: %d",
                 static_cast<int>(nBlockSize));
        return CE_Failure;
    }

    if (VSIFReadL(pData, static_cast<size_t>(nBlockSize), 1, fpData) != 1)
    {
        memset(pData, 0, nGDALBlockSize);

        if (fpData != fpExternal)
            CPLDebug("MMRBand", "Read of %x:%08x bytes at %d on %p failed.\n%s",
                     static_cast<int>(nBlockSize),
                     static_cast<int>(nBlockOffset >> 32),
                     static_cast<int>(nBlockOffset & 0xffffffff), fpData,
                     VSIStrerror(errno));

        return CE_None;
    }

    // Byte swap to local byte order if required.  It appears that
    // raster data is always stored in Intel byte order in Imagine
    // files.

#ifdef CPL_MSB
    if (MMRGetDataTypeBits(eDataType) == 16)
    {
        for (int ii = 0; ii < nBlockXSize * nBlockYSize; ii++)
            CPL_SWAP16PTR(((unsigned char *)pData) + ii * 2);
    }
    else if (MMRGetDataTypeBits(eDataType) == 32)
    {
        for (int ii = 0; ii < nBlockXSize * nBlockYSize; ii++)
            CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
    }
    else if (eDataType == EPT_f64)
    {
        for (int ii = 0; ii < nBlockXSize * nBlockYSize; ii++)
            CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8);
    }
    else if (eDataType == EPT_c64)
    {
        for (int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++)
            CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
    }
    else if (eDataType == EPT_c128)
    {
        for (int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++)
            CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8);
    }
#endif  // def CPL_MSB

    return CE_None;
}

/************************************************************************/
/*                           ReAllocBlock()                             */
/************************************************************************/

void MMRBand::ReAllocBlock(int iBlock, int nSize)
{
    // For compressed files - need to realloc the space for the block.

    // TODO: Should check to see if panBlockStart[iBlock] is not zero then do a
    // MMRFreeSpace() but that doesn't exist yet.
    // Instead as in interim measure it will reuse the existing block if
    // the new data will fit in.
    if ((panBlockStart[iBlock] != 0) && (nSize <= panBlockSize[iBlock]))
    {
        panBlockSize[iBlock] = nSize;
        // fprintf( stderr, "Reusing block %d\n", iBlock );
        return;
    }

    panBlockStart[iBlock] = MMRAllocateSpace(psInfo, nSize);

    panBlockSize[iBlock] = nSize;

    // Need to rewrite this info to the RasterDMS node.
    MMREntry *poDMS = poNode->GetNamedChild("RasterDMS");

    if (!poDMS)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Unable to load RasterDMS");
        return;
    }

    char szVarName[64];
    snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].offset", iBlock);
    poDMS->SetIntField(szVarName, static_cast<int>(panBlockStart[iBlock]));

    snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].size", iBlock);
    poDMS->SetIntField(szVarName, panBlockSize[iBlock]);
}

/************************************************************************/
/*                           SetRasterBlock()                           */
/************************************************************************/

CPLErr MMRBand::SetRasterBlock(int nXBlock, int nYBlock, void *pData)

{
    if (psInfo->eAccess == MMR_ReadOnly)
    {
        CPLError(
            CE_Failure, CPLE_NoWriteAccess,
            "Attempt to write block to read-only MiraMonRaster file failed.");
        return CE_Failure;
    }

    if (LoadBlockInfo() != CE_None)
        return CE_Failure;

    const int iBlock = nXBlock + nYBlock * nBlocksPerRow;

    // For now we don't support write invalid uncompressed blocks.
    // To do so we will need logic to make space at the end of the
    // file in the right size.
    if ((panBlockFlag[iBlock] & BFLG_VALID) == 0 &&
        !(panBlockFlag[iBlock] & BFLG_COMPRESSED) && panBlockStart[iBlock] == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to write to invalid tile with number %d "
                 "(X position %d, Y position %d).  This operation is "
                 "currently unsupported by MMRBand::SetRasterBlock().",
                 iBlock, nXBlock, nYBlock);

        return CE_Failure;
    }

    // Move to the location that the data sits.
    VSILFILE *fpData = nullptr;
    vsi_l_offset nBlockOffset = 0;

    // Calculate block offset in case we have spill file. Use predefined
    // block map otherwise.
    if (fpExternal)
    {
        fpData = fpExternal;
        nBlockOffset = nBlockStart + nBlockSize * iBlock * nLayerStackCount +
                       nLayerStackIndex * nBlockSize;
    }
    else
    {
        fpData = psInfo->fp;
        nBlockOffset = panBlockStart[iBlock];
        nBlockSize = panBlockSize[iBlock];
    }

    // Compressed Tile Handling.
    if (panBlockFlag[iBlock] & BFLG_COMPRESSED)
    {
        // Write compressed data.
        int nInBlockSize = static_cast<int>(
            (static_cast<GIntBig>(nBlockXSize) * nBlockYSize *
                 static_cast<GIntBig>(MMRGetDataTypeBits(eDataType)) +
             7) /
            8);

        // Create the compressor object.
        MMRCompress compress(pData, nInBlockSize, eDataType);
        if (compress.getCounts() == nullptr || compress.getValues() == nullptr)
        {
            return CE_Failure;
        }

        // Compress the data.
        if (compress.compressBlock())
        {
            // Get the data out of the object.
            GByte *pCounts = compress.getCounts();
            GUInt32 nSizeCount = compress.getCountSize();
            GByte *pValues = compress.getValues();
            GUInt32 nSizeValues = compress.getValueSize();
            GUInt32 nMin = compress.getMin();
            GUInt32 nNumRuns = compress.getNumRuns();
            GByte nNumBits = compress.getNumBits();

            // Compensate for the header info.
            GUInt32 nDataOffset = nSizeCount + 13;
            int nTotalSize = nSizeCount + nSizeValues + 13;

            // Allocate space for the compressed block and seek to it.
            ReAllocBlock(iBlock, nTotalSize);

            nBlockOffset = panBlockStart[iBlock];
            nBlockSize = panBlockSize[iBlock];

            // Seek to offset.
            if (VSIFSeekL(fpData, nBlockOffset, SEEK_SET) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Seek to %x:%08x on %p failed\n%s",
                         static_cast<int>(nBlockOffset >> 32),
                         static_cast<int>(nBlockOffset & 0xffffffff), fpData,
                         VSIStrerror(errno));
                return CE_Failure;
            }

            // Byte swap to local byte order if required.  It appears that
            // raster data is always stored in Intel byte order in Imagine
            // files.

#ifdef CPL_MSB
            CPL_SWAP32PTR(&nMin);
            CPL_SWAP32PTR(&nNumRuns);
            CPL_SWAP32PTR(&nDataOffset);
#endif  // def CPL_MSB

            // Write out the Minimum value.
            bool bRet = VSIFWriteL(&nMin, sizeof(nMin), 1, fpData) > 0;

            // The number of runs.
            bRet &= VSIFWriteL(&nNumRuns, sizeof(nNumRuns), 1, fpData) > 0;

            // The offset to the data.
            bRet &=
                VSIFWriteL(&nDataOffset, sizeof(nDataOffset), 1, fpData) > 0;

            // The number of bits.
            bRet &= VSIFWriteL(&nNumBits, sizeof(nNumBits), 1, fpData) > 0;

            // The counters - MSB stuff handled in MMRCompress.
            bRet &= VSIFWriteL(pCounts, nSizeCount, 1, fpData) > 0;

            // The values - MSB stuff handled in MMRCompress.
            bRet &= VSIFWriteL(pValues, nSizeValues, 1, fpData) > 0;

            if (!bRet)
                return CE_Failure;

            // Compressed data is freed in the MMRCompress destructor.
        }
        else
        {
            // If we have actually made the block bigger - i.e. does not
            // compress well.
            panBlockFlag[iBlock] ^= BFLG_COMPRESSED;
            // Alloc more space for the uncompressed block.
            ReAllocBlock(iBlock, nInBlockSize);

            nBlockOffset = panBlockStart[iBlock];
            nBlockSize = panBlockSize[iBlock];

            // Need to change the RasterDMS entry.
            MMREntry *poDMS = poNode->GetNamedChild("RasterDMS");

            if (!poDMS)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Unable to load RasterDMS");
                return CE_Failure;
            }

            char szVarName[64] = {};
            snprintf(szVarName, sizeof(szVarName),
                     "blockinfo[%d].compressionType", iBlock);
            poDMS->SetIntField(szVarName, 0);
        }

        // If the block was previously invalid, mark it as valid now.
        if ((panBlockFlag[iBlock] & BFLG_VALID) == 0)
        {
            char szVarName[64];
            MMREntry *poDMS = poNode->GetNamedChild("RasterDMS");

            if (!poDMS)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Unable to load RasterDMS");
                return CE_Failure;
            }

            snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].logvalid",
                     iBlock);
            poDMS->SetStringField(szVarName, "true");

            panBlockFlag[iBlock] |= BFLG_VALID;
        }
    }

    // Uncompressed TILE handling.
    if ((panBlockFlag[iBlock] & BFLG_COMPRESSED) == 0)
    {

        if (VSIFSeekL(fpData, nBlockOffset, SEEK_SET) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Seek to %x:%08x on %p failed\n%s",
                     static_cast<int>(nBlockOffset >> 32),
                     static_cast<int>(nBlockOffset & 0xffffffff), fpData,
                     VSIStrerror(errno));
            return CE_Failure;
        }

        // Byte swap to local byte order if required.  It appears that
        // raster data is always stored in Intel byte order in Imagine
        // files.

#ifdef CPL_MSB
        if (MMRGetDataTypeBits(eDataType) == 16)
        {
            for (int ii = 0; ii < nBlockXSize * nBlockYSize; ii++)
                CPL_SWAP16PTR(((unsigned char *)pData) + ii * 2);
        }
        else if (MMRGetDataTypeBits(eDataType) == 32)
        {
            for (int ii = 0; ii < nBlockXSize * nBlockYSize; ii++)
                CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
        }
        else if (eDataType == EPT_f64)
        {
            for (int ii = 0; ii < nBlockXSize * nBlockYSize; ii++)
                CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8);
        }
        else if (eDataType == EPT_c64)
        {
            for (int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++)
                CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
        }
        else if (eDataType == EPT_c128)
        {
            for (int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++)
                CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8);
        }
#endif  // def CPL_MSB

        // Write uncompressed data.
        if (VSIFWriteL(pData, static_cast<size_t>(nBlockSize), 1, fpData) != 1)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Write of %d bytes at %x:%08x on %p failed.\n%s",
                     static_cast<int>(nBlockSize),
                     static_cast<int>(nBlockOffset >> 32),
                     static_cast<int>(nBlockOffset & 0xffffffff), fpData,
                     VSIStrerror(errno));
            return CE_Failure;
        }

        // If the block was previously invalid, mark it as valid now.
        if ((panBlockFlag[iBlock] & BFLG_VALID) == 0)
        {
            char szVarName[64];
            MMREntry *poDMS = poNode->GetNamedChild("RasterDMS");
            if (poDMS == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to get RasterDMS when trying to mark "
                         "block valid.");
                return CE_Failure;
            }
            snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].logvalid",
                     iBlock);
            poDMS->SetStringField(szVarName, "true");

            panBlockFlag[iBlock] |= BFLG_VALID;
        }
    }
    // Swap back, since we don't really have permission to change
    // the callers buffer.

#ifdef CPL_MSB
    if (MMRGetDataTypeBits(eDataType) == 16)
    {
        for (int ii = 0; ii < nBlockXSize * nBlockYSize; ii++)
            CPL_SWAP16PTR(((unsigned char *)pData) + ii * 2);
    }
    else if (MMRGetDataTypeBits(eDataType) == 32)
    {
        for (int ii = 0; ii < nBlockXSize * nBlockYSize; ii++)
            CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
    }
    else if (eDataType == EPT_f64)
    {
        for (int ii = 0; ii < nBlockXSize * nBlockYSize; ii++)
            CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8);
    }
    else if (eDataType == EPT_c64)
    {
        for (int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++)
            CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
    }
    else if (eDataType == EPT_c128)
    {
        for (int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++)
            CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8);
    }
#endif  // def CPL_MSB

    return CE_None;
}

/************************************************************************/
/*                         GetBandName()                                */
/*                                                                      */
/*      Return the Layer Name                                           */
/************************************************************************/

const char *MMRBand::GetBandName()
{
    if (strlen(poNode->GetName()) > 0)
        return poNode->GetName();

    for (int iBand = 0; iBand < psInfo->nBands; iBand++)
    {
        if (psInfo->papoBand[iBand] == this)
        {
            osOverName.Printf("Layer_%d", iBand + 1);
            return osOverName;
        }
    }

    osOverName.Printf("Layer_%x", poNode->GetFilePos());
    return osOverName;
}

/************************************************************************/
/*                         SetBandName()                                */
/*                                                                      */
/*      Set the Layer Name                                              */
/************************************************************************/

void MMRBand::SetBandName(const char *pszName)
{
    if (psInfo->eAccess == MMR_Update)
    {
        poNode->SetName(pszName);
    }
}

/************************************************************************/
/*                         SetNoDataValue()                             */
/*                                                                      */
/*      Set the band no-data value                                      */
/************************************************************************/

CPLErr MMRBand::SetNoDataValue(double dfValue)
{
    if (psInfo->eAccess != MMR_Update)
        return CE_Failure;

    MMREntry *poNDNode = poNode->GetNamedChild("Eimg_NonInitializedValue");

    if (poNDNode == nullptr)
    {
        poNDNode = MMREntry::New(psInfo, "Eimg_NonInitializedValue",
                                 "Eimg_NonInitializedValue", poNode);
    }

    poNDNode->MakeData(8 + 12 + 8);
    poNDNode->SetPosition();

    poNDNode->SetIntField("valueBD[-3]", EPT_f64);
    poNDNode->SetIntField("valueBD[-2]", 1);
    poNDNode->SetIntField("valueBD[-1]", 1);

    if (poNDNode->SetDoubleField("valueBD[0]", dfValue) == CE_Failure)
        return CE_Failure;

    bNoDataSet = true;
    dfNoData = dfValue;
    return CE_None;
}

/************************************************************************/
/*                        MMRReadBFUniqueBins()                         */
/*                                                                      */
/*      Attempt to read the bins used for a PCT or RAT from a           */
/*      BinFunction node.  On failure just return NULL.                 */
/************************************************************************/

double *MMRReadBFUniqueBins(MMREntry *poBinFunc, int nPCTColors)

{
    // First confirm this is a "BFUnique" bin function.  We don't
    // know what to do with any other types.
    const char *pszBinFunctionType =
        poBinFunc->GetStringField("binFunction.type.string");

    if (pszBinFunctionType == nullptr || !EQUAL(pszBinFunctionType, "BFUnique"))
        return nullptr;

    // Process dictionary.
    const char *pszDict =
        poBinFunc->GetStringField("binFunction.MIFDictionary.string");
    if (pszDict == nullptr)
        pszDict = poBinFunc->GetStringField("binFunction.MIFDictionary");
    if (pszDict == nullptr)
        return nullptr;

    MMRDictionary oMiniDict(pszDict);

    MMRType *poBFUnique = oMiniDict.FindType("BFUnique");
    if (poBFUnique == nullptr)
        return nullptr;

    // Field the MIFObject raw data pointer.
    int nMIFObjectSize = 0;
    const GByte *pabyMIFObject =
        reinterpret_cast<const GByte *>(poBinFunc->GetStringField(
            "binFunction.MIFObject", nullptr, &nMIFObjectSize));

    if (pabyMIFObject == nullptr ||
        nMIFObjectSize < 24 + static_cast<int>(sizeof(double)) * nPCTColors)
        return nullptr;

    // Confirm that this is a 64bit floating point basearray.
    if (pabyMIFObject[20] != 0x0a || pabyMIFObject[21] != 0x00)
    {
        CPLDebug("MiraMonRaster",
                 "MMRReadPCTBins(): "
                 "The basedata does not appear to be EGDA_TYPE_F64.");
        return nullptr;
    }

    // Decode bins.
    double *padfBins =
        static_cast<double *>(CPLCalloc(sizeof(double), nPCTColors));

    memcpy(padfBins, pabyMIFObject + 24, sizeof(double) * nPCTColors);

    for (int i = 0; i < nPCTColors; i++)
    {
        MMRStandard(8, padfBins + i);
#if DEBUG_VERBOSE
        CPLDebug("MiraMonRaster", "Bin[%d] = %g", i, padfBins[i]);
#endif
    }

    return padfBins;
}

/************************************************************************/
/*                               GetPCT()                               */
/*                                                                      */
/*      Return PCT information, if any exists.                          */
/************************************************************************/

CPLErr MMRBand::GetPCT(int *pnColors, double **ppadfRed, double **ppadfGreen,
                       double **ppadfBlue, double **ppadfAlpha,
                       double **ppadfBins)

{
    *pnColors = 0;
    *ppadfRed = nullptr;
    *ppadfGreen = nullptr;
    *ppadfBlue = nullptr;
    *ppadfAlpha = nullptr;
    *ppadfBins = nullptr;

    // If we haven't already tried to load the colors, do so now.
    if (nPCTColors == -1)
    {

        nPCTColors = 0;

        // ·$·TODO Lectura de la DBF
        /*
        MMREntry *poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Red");
        if (poColumnEntry == nullptr)
            return CE_Failure;

        nPCTColors = poColumnEntry->GetIntField("numRows");
        if (nPCTColors < 0 || nPCTColors > 65536)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid number of colors: %d", nPCTColors);
            return CE_Failure;
        }

        for (int iColumn = 0; iColumn < 4; iColumn++)
        {
            apadfPCT[iColumn] = static_cast<double *>(
                VSI_MALLOC2_VERBOSE(sizeof(double), nPCTColors));
            if (apadfPCT[iColumn] == nullptr)
            {
                return CE_Failure;
            }

            if (iColumn == 0)
            {
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Red");
            }
            else if (iColumn == 1)
            {
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Green");
            }
            else if (iColumn == 2)
            {
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Blue");
            }
            else if (iColumn == 3)
            {
                poColumnEntry =
                    poNode->GetNamedChild("Descriptor_Table.Opacity");
            }

            if (poColumnEntry == nullptr)
            {
                double *pdCol = apadfPCT[iColumn];
                for (int i = 0; i < nPCTColors; i++)
                    pdCol[i] = 1.0;
            }
            else
            {
                if (VSIFSeekL(psInfo->fp,
                              poColumnEntry->GetIntField("columnDataPtr"),
                              SEEK_SET) < 0)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "VSIFSeekL() failed in MMRBand::GetPCT().");
                    return CE_Failure;
                }
                if (VSIFReadL(apadfPCT[iColumn], sizeof(double), nPCTColors,
                              psInfo->fp) != static_cast<size_t>(nPCTColors))
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "VSIFReadL() failed in MMRBand::GetPCT().");
                    return CE_Failure;
                }

                for (int i = 0; i < nPCTColors; i++)
                    MMRStandard(8, apadfPCT[iColumn] + i);
            }
        }

        // Do we have a custom binning function? If so, try reading it.
        MMREntry *poBinFunc =
            poNode->GetNamedChild("Descriptor_Table.#Bin_Function840#");

        if (poBinFunc != nullptr)
        {
            padfPCTBins = MMRReadBFUniqueBins(poBinFunc, nPCTColors);
        }
        */
    }

    // Return the values.
    //if (nPCTColors == 0)
    //    return CE_Failure;

    *pnColors = nPCTColors;
    *ppadfRed = apadfPCT[0];
    *ppadfGreen = apadfPCT[1];
    *ppadfBlue = apadfPCT[2];
    *ppadfAlpha = apadfPCT[3];
    *ppadfBins = padfPCTBins;

    return CE_None;
}

/************************************************************************/
/*                               SetPCT()                               */
/*                                                                      */
/*      Set the PCT information for this band.                          */
/************************************************************************/

CPLErr MMRBand::SetPCT(int nColors, const double *padfRed,
                       const double *padfGreen, const double *padfBlue,
                       const double *padfAlpha)

{
    static const char *const apszColNames[4] = {"Red", "Green", "Blue",
                                                "Opacity"};
    const double *const apadfValues[] = {padfRed, padfGreen, padfBlue,
                                         padfAlpha};
    MMREntry *poEdsc_Table;

    // Do we need to try and clear any existing color table?
    if (nColors == 0)
    {
        poEdsc_Table = poNode->GetNamedChild("Descriptor_Table");
        if (poEdsc_Table == nullptr)
            return CE_None;

        for (int iColumn = 0; iColumn < 4; iColumn++)
        {
            MMREntry *poEdsc_Column =
                poEdsc_Table->GetNamedChild(apszColNames[iColumn]);
            if (poEdsc_Column)
                poEdsc_Column->RemoveAndDestroy();
        }

        return CE_None;
    }

    // Create the Descriptor table.
    poEdsc_Table = poNode->GetNamedChild("Descriptor_Table");
    if (poEdsc_Table == nullptr ||
        !EQUAL(poEdsc_Table->GetType(), "Edsc_Table"))
        poEdsc_Table =
            MMREntry::New(psInfo, "Descriptor_Table", "Edsc_Table", poNode);

    poEdsc_Table->SetIntField("numrows", nColors);

    // Create the Binning function node.  I am not sure that we
    // really need this though.
    MMREntry *poEdsc_BinFunction =
        poEdsc_Table->GetNamedChild("#Bin_Function#");
    if (poEdsc_BinFunction == nullptr ||
        !EQUAL(poEdsc_BinFunction->GetType(), "Edsc_BinFunction"))
        poEdsc_BinFunction = MMREntry::New(psInfo, "#Bin_Function#",
                                           "Edsc_BinFunction", poEdsc_Table);

    // Because of the BaseData we have to hardcode the size.
    poEdsc_BinFunction->MakeData(30);

    poEdsc_BinFunction->SetIntField("numBins", nColors);
    poEdsc_BinFunction->SetStringField("binFunction", "direct");
    poEdsc_BinFunction->SetDoubleField("minLimit", 0.0);
    poEdsc_BinFunction->SetDoubleField("maxLimit", nColors - 1.0);

    // Process each color component.
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        const double *padfValues = apadfValues[iColumn];
        const char *pszName = apszColNames[iColumn];

        // Create the Edsc_Column.
        MMREntry *poEdsc_Column = poEdsc_Table->GetNamedChild(pszName);
        if (poEdsc_Column == nullptr ||
            !EQUAL(poEdsc_Column->GetType(), "Edsc_Column"))
            poEdsc_Column =
                MMREntry::New(psInfo, pszName, "Edsc_Column", poEdsc_Table);

        poEdsc_Column->SetIntField("numRows", nColors);
        poEdsc_Column->SetStringField("dataType", "real");
        poEdsc_Column->SetIntField("maxNumChars", 0);

        // Write the data out.
        const int nOffset = MMRAllocateSpace(psInfo, 8 * nColors);

        poEdsc_Column->SetIntField("columnDataPtr", nOffset);

        double *padfFileData =
            static_cast<double *>(CPLMalloc(nColors * sizeof(double)));
        for (int iColor = 0; iColor < nColors; iColor++)
        {
            padfFileData[iColor] = padfValues[iColor];
            MMRStandard(8, padfFileData + iColor);
        }
        const bool bRet = VSIFSeekL(psInfo->fp, nOffset, SEEK_SET) >= 0 &&
                          VSIFWriteL(padfFileData, 8, nColors, psInfo->fp) ==
                              static_cast<size_t>(nColors);
        CPLFree(padfFileData);
        if (!bRet)
            return CE_Failure;
    }

    // Update the layer type to be thematic.
    poNode->SetStringField("layerType", "thematic");

    return CE_None;
}
