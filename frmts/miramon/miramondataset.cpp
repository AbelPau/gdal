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
#include "miramondataset.h"
#include "miramon_p.h"
#include "miramonrel.h"
#include "miramon_rastertools.h"

//#include <cassert>
/*
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_rat.h"
#include "miramon.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
*/

#ifdef MSVC
#include "..\miramon_common\mm_gdal_functions.h"  // For MMCheck_REL_FILE()
#else
#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()
#endif

/************************************************************************/
/*                           MMRRasterBand()                            */
/************************************************************************/
MMRRasterBand::MMRRasterBand(MMRDataset *poDSIn, int nBandIn)
    : poCT(nullptr), eMMRDataType(EPT_MIN), hMMR(poDSIn->hMMR),
      bMetadataDirty(false), poDefaultRAT(nullptr)
{
    poDS = poDSIn;

    nBand = nBandIn;
    eAccess = poDSIn->GetAccess();

    int nCompression = 0;
    MMRGetBandInfo(hMMR, nBand, &eMMRDataTypeMiraMon, &eMMBytesPerPixel,
                   &nBlockXSize, &nBlockYSize, &nCompression);

    // Set some other information.
    if (nCompression != 0)
        GDALMajorObject::SetMetadataItem("COMPRESSION", "RLE",
                                         "IMAGE_STRUCTURE");
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
                     (int)eMMRDataTypeMiraMon);
            break;
    }

    // Collect color table if present.
    CPLErr eErr = MMRGetPCT(hMMR, nBand);
    int nColors =
        static_cast<int>(hMMR->papoBand[nBand - 1]->GetPCT_Red().size());

    if (eErr == CE_None && nColors > 0)
    {
        poCT = new GDALColorTable(GPI_RGB);
        for (int iColor = 0; iColor < nColors; iColor++)
        {
            GDALColorEntry sEntry = {
                (short int)(hMMR->papoBand[nBand - 1]->GetPCT_Red()[iColor]),
                (short int)(hMMR->papoBand[nBand - 1]->GetPCT_Green()[iColor]),
                (short int)(hMMR->papoBand[nBand - 1]->GetPCT_Blue()[iColor]),
                (short int)(hMMR->papoBand[nBand - 1]->GetPCT_Alpha()[iColor])};

            if ((sEntry.c1 < 0 || sEntry.c1 > 255) ||
                (sEntry.c2 < 0 || sEntry.c2 > 255) ||
                (sEntry.c3 < 0 || sEntry.c3 > 255))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Color table entry appears to be corrupt, skipping "
                         "the rest. ");
                break;
            }

            poCT->SetColorEntry(iColor, &sEntry);
        }
    }
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
/*                             GetNoDataValue()                         */
/************************************************************************/

double MMRRasterBand::GetNoDataValue(int *pbSuccess)

{
    double dfNoData = 0.0;

    if (MMRGetBandNoData(hMMR, nBand, &dfNoData))
    {
        if (pbSuccess)
            *pbSuccess = TRUE;
        return dfNoData;
    }
    if (pbSuccess)
        *pbSuccess = FALSE;
    return dfNoData;

    //return GDALPamRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                             SetNoDataValue()                         */
/************************************************************************/

CPLErr MMRRasterBand::SetNoDataValue(double dfValue)
{
    return MMRSetBandNoData(hMMR, nBand, dfValue);
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

    eErr = MMRGetRasterBlockEx(hMMR, nBand, nBlockXOff, nBlockYOff, pImage,
                               nBlockXSize * nBlockYSize *
                                   GDALGetDataTypeSizeBytes(eDataType));

    if (eErr == CE_None &&
        eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BIT)
    {
        // ·$·TODO Revisar
        GByte *pabyData = static_cast<GByte *>(pImage);

        for (int ii = nBlockXSize * nBlockYSize - 1; ii >= 0; ii--)
        {
            if ((pabyData[ii >> 3] & (1 << (ii & 0x7))))
                pabyData[ii] = 1;
            else
                pabyData[ii] = 0;
        }
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr MMRRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    GByte *pabyOutBuf = static_cast<GByte *>(pImage);

    // Do we need to pack 1/2/4 bit data?
    // TODO(schwehr): Make symbolic constants with explanations.
    if (eMMRDataType == EPT_u1 || eMMRDataType == EPT_u2 ||
        eMMRDataType == EPT_u4)
    {
        const int nPixCount = nBlockXSize * nBlockYSize;
        pabyOutBuf = static_cast<GByte *>(VSIMalloc2(nBlockXSize, nBlockYSize));
        if (pabyOutBuf == nullptr)
            return CE_Failure;

        if (eMMRDataType == EPT_u1)
        {
            for (int ii = 0; ii < nPixCount - 7; ii += 8)
            {
                const int k = ii >> 3;
                // TODO(schwehr): Create a temp for (GByte *)pImage.
                pabyOutBuf[k] = (((GByte *)pImage)[ii] & 0x1) |
                                ((((GByte *)pImage)[ii + 1] & 0x1) << 1) |
                                ((((GByte *)pImage)[ii + 2] & 0x1) << 2) |
                                ((((GByte *)pImage)[ii + 3] & 0x1) << 3) |
                                ((((GByte *)pImage)[ii + 4] & 0x1) << 4) |
                                ((((GByte *)pImage)[ii + 5] & 0x1) << 5) |
                                ((((GByte *)pImage)[ii + 6] & 0x1) << 6) |
                                ((((GByte *)pImage)[ii + 7] & 0x1) << 7);
            }
        }
        else if (eMMRDataType == EPT_u2)
        {
            for (int ii = 0; ii < nPixCount - 3; ii += 4)
            {
                const int k = ii >> 2;
                pabyOutBuf[k] = (((GByte *)pImage)[ii] & 0x3) |
                                ((((GByte *)pImage)[ii + 1] & 0x3) << 2) |
                                ((((GByte *)pImage)[ii + 2] & 0x3) << 4) |
                                ((((GByte *)pImage)[ii + 3] & 0x3) << 6);
            }
        }
        else if (eMMRDataType == EPT_u4)
        {
            for (int ii = 0; ii < nPixCount - 1; ii += 2)
            {
                const int k = ii >> 1;
                pabyOutBuf[k] = (((GByte *)pImage)[ii] & 0xf) |
                                ((((GByte *)pImage)[ii + 1] & 0xf) << 4);
            }
        }
    }

    // Actually write out.
    const CPLErr nRetCode =
        MMRSetRasterBlock(hMMR, nBand, nBlockXOff, nBlockYOff, pabyOutBuf);

    if (pabyOutBuf != pImage)
        CPLFree(pabyOutBuf);

    return nRetCode;
}

/************************************************************************/
/*                         GetDescription()                             */
/************************************************************************/

const char *MMRRasterBand::GetDescription() const
{
    return MMRGetBandName(hMMR, nBand);
    /*
    const char *pszName = MMRGetBandName(hMMR, nBand);

    if (pszName == nullptr)
        return GDALPamRasterBand::GetDescription();

    return pszName;
    */
}

/************************************************************************/
/*                         MMRGetBandName()                             */
/************************************************************************/

const char *MMRGetBandName(MMRHandle hMMR, int nBand)
{
    if (nBand < 1 || nBand > hMMR->nBands)
        return "";

    return hMMR->papoBand[nBand - 1]->GetBandName();
}

/************************************************************************/
/*                         SetDescription()                             */
/************************************************************************/
void MMRRasterBand::SetDescription(const char *pszName)
{
    if (strlen(pszName) > 0)
        MMRSetBandName(hMMR, nBand, pszName);
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
    return poCT;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr MMRRasterBand::SetColorTable(GDALColorTable *poCTable)

{
    if (GetAccess() == GA_ReadOnly)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Unable to set color table on read-only file.");
        return CE_Failure;
    }

    // Special case if we are clearing the color table.
    if (poCTable == nullptr)
    {
        delete poCT;
        poCT = nullptr;

        MMRSetPCT(hMMR, nBand, 0, nullptr, nullptr, nullptr, nullptr);

        return CE_None;
    }

    // Write out the colortable, and update the configuration.
    int nColors = poCTable->GetColorEntryCount();

    /* -------------------------------------------------------------------- */
    /*      If we already have a non-empty RAT set and it's smaller than    */
    /*      the colour table, and all the trailing CT entries are the same, */
    /*      truncate the colour table. Helps when RATs travel via GTiff.    */
    /* -------------------------------------------------------------------- */
    const GDALRasterAttributeTable *poRAT = GetDefaultRAT();
    if (poRAT != nullptr && poRAT->GetRowCount() > 0 &&
        poRAT->GetRowCount() < nColors)
    {
        bool match = true;
        const GDALColorEntry *color1 =
            poCTable->GetColorEntry(poRAT->GetRowCount());
        for (int i = poRAT->GetRowCount() + 1; match && i < nColors; i++)
        {
            const GDALColorEntry *color2 = poCTable->GetColorEntry(i);
            match = (color1->c1 == color2->c1 && color1->c2 == color2->c2 &&
                     color1->c3 == color2->c3 && color1->c4 == color2->c4);
        }
        if (match)
        {
            CPLDebug("MiraMonRaster",
                     "SetColorTable: Truncating PCT size (%d) to RAT size (%d)",
                     nColors, poRAT->GetRowCount());
            nColors = poRAT->GetRowCount();
        }
    }

    double *padfRed =
        static_cast<double *>(CPLMalloc(sizeof(double) * nColors));
    double *padfGreen =
        static_cast<double *>(CPLMalloc(sizeof(double) * nColors));
    double *padfBlue =
        static_cast<double *>(CPLMalloc(sizeof(double) * nColors));
    double *padfAlpha =
        static_cast<double *>(CPLMalloc(sizeof(double) * nColors));

    for (int iColor = 0; iColor < nColors; iColor++)
    {
        GDALColorEntry sRGB;

        poCTable->GetColorEntryAsRGB(iColor, &sRGB);

        padfRed[iColor] = sRGB.c1 / 255.0;
        padfGreen[iColor] = sRGB.c2 / 255.0;
        padfBlue[iColor] = sRGB.c3 / 255.0;
        padfAlpha[iColor] = sRGB.c4 / 255.0;
    }

    MMRSetPCT(hMMR, nBand, nColors, padfRed, padfGreen, padfBlue, padfAlpha);

    CPLFree(padfRed);
    CPLFree(padfGreen);
    CPLFree(padfBlue);
    CPLFree(padfAlpha);

    if (poCT)
        delete poCT;

    poCT = poCTable->Clone();

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
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr MMRRasterBand::SetMetadataItem(const char *pszTag, const char *pszValue,
                                      const char *pszDomain)

{
    bMetadataDirty = true;

    return GDALPamRasterBand::SetMetadataItem(pszTag, pszValue, pszDomain);
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

CPLErr MMRRasterBand::GetDefaultHistogram(double *pdfMin, double *pdfMax,
                                          int *pnBuckets,
                                          GUIntBig **ppanHistogram, int bForce,
                                          GDALProgressFunc pfnProgress,
                                          void *pProgressData)

{
    if (GetMetadataItem("STATISTICS_HISTOBINVALUES") != nullptr &&
        GetMetadataItem("STATISTICS_HISTOMIN") != nullptr &&
        GetMetadataItem("STATISTICS_HISTOMAX") != nullptr)
    {
        const char *pszBinValues = GetMetadataItem("STATISTICS_HISTOBINVALUES");

        *pdfMin = CPLAtof(GetMetadataItem("STATISTICS_HISTOMIN"));
        *pdfMax = CPLAtof(GetMetadataItem("STATISTICS_HISTOMAX"));

        *pnBuckets = 0;
        for (int i = 0; pszBinValues[i] != '\0'; i++)
        {
            if (pszBinValues[i] == '|')
                (*pnBuckets)++;
        }

        *ppanHistogram =
            static_cast<GUIntBig *>(CPLCalloc(sizeof(GUIntBig), *pnBuckets));

        const char *pszNextBin = pszBinValues;
        for (int i = 0; i < *pnBuckets; i++)
        {
            (*ppanHistogram)[i] =
                static_cast<GUIntBig>(CPLAtoGIntBig(pszNextBin));

            while (*pszNextBin != '|' && *pszNextBin != '\0')
                pszNextBin++;
            if (*pszNextBin == '|')
                pszNextBin++;
        }

        // Adjust min/max to reflect outer edges of buckets.
        double dfBucketWidth = (*pdfMax - *pdfMin) / (*pnBuckets - 1);
        *pdfMax += 0.5 * dfBucketWidth;
        *pdfMin -= 0.5 * dfBucketWidth;

        return CE_None;
    }

    return GDALPamRasterBand::GetDefaultHistogram(pdfMin, pdfMax, pnBuckets,
                                                  ppanHistogram, bForce,
                                                  pfnProgress, pProgressData);
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

CPLErr MMRRasterBand::SetDefaultRAT(const GDALRasterAttributeTable *poRAT)

{
    if (poRAT == nullptr)
        return CE_Failure;

    delete poDefaultRAT;
    poDefaultRAT = nullptr;

    CPLErr r = WriteNamedRAT("Descriptor_Table", poRAT);
    if (!r)
        GetDefaultRAT();

    return r;
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *MMRRasterBand::GetDefaultRAT()

{
    //·$·TODO de moment:
    return nullptr;
    /*if (poDefaultRAT == nullptr)
        poDefaultRAT = new MMRRasterAttributeTable(this, "Descriptor_Table");

    return poDefaultRAT;
    */
}

/************************************************************************/
/*                            WriteNamedRAT()                            */
/************************************************************************/

CPLErr MMRRasterBand::WriteNamedRAT(const char * /*pszName*/,
                                    const GDALRasterAttributeTable *poRAT)
{
    // Find the requested table.
    MMREntry *poDT =
        hMMR->papoBand[nBand - 1]->poNode->GetNamedChild("Descriptor_Table");
    if (poDT == nullptr || !EQUAL(poDT->GetType(), "Edsc_Table"))
        poDT =
            MMREntry::New(hMMR->papoBand[nBand - 1]->psInfo, "Descriptor_Table",
                          "Edsc_Table", hMMR->papoBand[nBand - 1]->poNode);

    const int nRowCount = poRAT->GetRowCount();

    poDT->SetIntField("numrows", nRowCount);
    // Check if binning is set on this RAT.
    double dfBinSize = 0.0;
    double dfRow0Min = 0.0;
    if (poRAT->GetLinearBinning(&dfRow0Min, &dfBinSize))
    {
        // Then it should have an Edsc_BinFunction.
        MMREntry *poBinFunction = poDT->GetNamedChild("#Bin_Function#");
        if (poBinFunction == nullptr ||
            !EQUAL(poBinFunction->GetType(), "Edsc_BinFunction"))
        {
            poBinFunction =
                MMREntry::New(hMMR->papoBand[nBand - 1]->psInfo,
                              "#Bin_Function#", "Edsc_BinFunction", poDT);
        }

        // direct for thematic layers, linear otherwise
        const char *pszLayerType =
            hMMR->papoBand[nBand - 1]->poNode->GetStringField("layerType");
        if (pszLayerType == nullptr || STARTS_WITH_CI(pszLayerType, "thematic"))
            poBinFunction->SetStringField("binFunctionType", "direct");
        else
            poBinFunction->SetStringField("binFunctionType", "linear");

        poBinFunction->SetDoubleField("minLimit", dfRow0Min);
        poBinFunction->SetDoubleField("maxLimit",
                                      (nRowCount - 1) * dfBinSize + dfRow0Min);
        poBinFunction->SetIntField("numBins", nRowCount);
    }

    // Loop through each column in the RAT.
    for (int col = 0; col < poRAT->GetColumnCount(); col++)
    {
        const char *pszName = nullptr;

        if (poRAT->GetUsageOfCol(col) == GFU_Red)
        {
            pszName = "Red";
        }
        else if (poRAT->GetUsageOfCol(col) == GFU_Green)
        {
            pszName = "Green";
        }
        else if (poRAT->GetUsageOfCol(col) == GFU_Blue)
        {
            pszName = "Blue";
        }
        else if (poRAT->GetUsageOfCol(col) == GFU_Alpha)
        {
            pszName = "Opacity";
        }
        else if (poRAT->GetUsageOfCol(col) == GFU_PixelCount)
        {
            pszName = "Histogram";
        }
        else if (poRAT->GetUsageOfCol(col) == GFU_Name)
        {
            pszName = "Class_Names";
        }
        else
        {
            pszName = poRAT->GetNameOfCol(col);
        }

        // Check to see if a column with pszName exists and create if
        // if necessary.
        MMREntry *poColumn = poDT->GetNamedChild(pszName);

        if (poColumn == nullptr || !EQUAL(poColumn->GetType(), "Edsc_Column"))
            poColumn = MMREntry::New(hMMR->papoBand[nBand - 1]->psInfo, pszName,
                                     "Edsc_Column", poDT);

        poColumn->SetIntField("numRows", nRowCount);
        // Color cols which are integer in GDAL are written as floats in MMR.
        bool bIsColorCol = false;
        if (poRAT->GetUsageOfCol(col) == GFU_Red ||
            poRAT->GetUsageOfCol(col) == GFU_Green ||
            poRAT->GetUsageOfCol(col) == GFU_Blue ||
            poRAT->GetUsageOfCol(col) == GFU_Alpha)
        {
            bIsColorCol = true;
        }

        // Write float also if a color column or histogram.
        if (poRAT->GetTypeOfCol(col) == GFT_Real || bIsColorCol ||
            poRAT->GetUsageOfCol(col) == GFU_PixelCount)
        {
            const int nOffset =
                MMRAllocateSpace(hMMR->papoBand[nBand - 1]->psInfo,
                                 static_cast<GUInt32>(nRowCount) *
                                     static_cast<GUInt32>(sizeof(double)));
            poColumn->SetIntField("columnDataPtr", nOffset);
            poColumn->SetStringField("dataType", "real");

            double *padfColData =
                static_cast<double *>(CPLCalloc(nRowCount, sizeof(double)));
            for (int i = 0; i < nRowCount; i++)
            {
                if (bIsColorCol)
                    // Stored 0..1
                    padfColData[i] = poRAT->GetValueAsInt(i, col) / 255.0;
                else
                    padfColData[i] = poRAT->GetValueAsDouble(i, col);
            }
#ifdef CPL_MSB
            GDALSwapWords(padfColData, 8, nRowCount, 8);
#endif
            if (VSIFSeekL(hMMR->fp, nOffset, SEEK_SET) != 0 ||
                VSIFWriteL(padfColData, nRowCount, sizeof(double), hMMR->fp) !=
                    sizeof(double))
            {
                CPLError(CE_Failure, CPLE_FileIO, "WriteNamedRAT() failed");
                CPLFree(padfColData);
                return CE_Failure;
            }
            CPLFree(padfColData);
        }
        else if (poRAT->GetTypeOfCol(col) == GFT_String)
        {
            unsigned int nMaxNumChars = 0;
            // Find the length of the longest string.
            for (int i = 0; i < nRowCount; i++)
            {
                // Include terminating byte.
                const unsigned int nNumChars = static_cast<unsigned int>(
                    strlen(poRAT->GetValueAsString(i, col)) + 1);
                if (nMaxNumChars < nNumChars)
                {
                    nMaxNumChars = nNumChars;
                }
            }

            const int nOffset =
                MMRAllocateSpace(hMMR->papoBand[nBand - 1]->psInfo,
                                 (nRowCount + 1) * nMaxNumChars);
            poColumn->SetIntField("columnDataPtr", nOffset);
            poColumn->SetStringField("dataType", "string");
            poColumn->SetIntField("maxNumChars", nMaxNumChars);

            char *pachColData =
                static_cast<char *>(CPLCalloc(nRowCount + 1, nMaxNumChars));
            for (int i = 0; i < nRowCount; i++)
            {
                strcpy(&pachColData[nMaxNumChars * i],
                       poRAT->GetValueAsString(i, col));
            }
            if (VSIFSeekL(hMMR->fp, nOffset, SEEK_SET) != 0 ||
                VSIFWriteL(pachColData, nRowCount, nMaxNumChars, hMMR->fp) !=
                    nMaxNumChars)
            {
                CPLError(CE_Failure, CPLE_FileIO, "WriteNamedRAT() failed");
                CPLFree(pachColData);
                return CE_Failure;
            }
            CPLFree(pachColData);
        }
        else if (poRAT->GetTypeOfCol(col) == GFT_Integer)
        {
            const int nOffset = MMRAllocateSpace(
                hMMR->papoBand[nBand - 1]->psInfo,
                static_cast<GUInt32>(nRowCount) * (GUInt32)sizeof(GInt32));
            poColumn->SetIntField("columnDataPtr", nOffset);
            poColumn->SetStringField("dataType", "integer");

            GInt32 *panColData =
                static_cast<GInt32 *>(CPLCalloc(nRowCount, sizeof(GInt32)));
            for (int i = 0; i < nRowCount; i++)
            {
                panColData[i] = poRAT->GetValueAsInt(i, col);
            }
#ifdef CPL_MSB
            GDALSwapWords(panColData, 4, nRowCount, 4);
#endif
            if (VSIFSeekL(hMMR->fp, nOffset, SEEK_SET) != 0 ||
                VSIFWriteL(panColData, nRowCount, sizeof(GInt32), hMMR->fp) !=
                    sizeof(GInt32))
            {
                CPLError(CE_Failure, CPLE_FileIO, "WriteNamedRAT() failed");
                CPLFree(panColData);
                return CE_Failure;
            }
            CPLFree(panColData);
        }
        else
        {
            // Can't deal with any of the others yet.
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Writing this data type in a column is not supported "
                     "for this Raster Attribute Table.");
        }
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                            MMRDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            MMRDataset()                            */
/************************************************************************/

MMRDataset::MMRDataset()
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    memset(adfGeoTransform, 0, sizeof(adfGeoTransform));

    nNSubdataSets = 0;
}

/************************************************************************/
/*                           ~MMRDataset()                            */
/************************************************************************/

MMRDataset::~MMRDataset()

{
    FlushCache(true);

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

    // Close the file.
    if (hMMR != nullptr)
    {
        if (MMRClose(hMMR) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
        hMMR = nullptr;
    }
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
    if (MMRRel::IdentifySubdataSetFile(poOpenInfo->pszFilename))
        return TRUE;

    return MMRRel::IdentifyFile(poOpenInfo->pszFilename);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *MMRDataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Verify that this is a MMR file.
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // During fuzzing, do not use Identify to reject crazy content.
    if (!Identify(poOpenInfo))
        return nullptr;
#endif

    // Creating of the object that allows inspect metadata (REL file)
    MMRRel *fRel = new MMRRel(poOpenInfo->pszFilename);

    // Getting the info fromthat REL
    MMRHandle hMMR =
        fRel->GetInfoFromREL(poOpenInfo->pszFilename,
                             (poOpenInfo->eAccess == GA_Update ? "r+" : "r"));
    if (!hMMR)
    {
        delete fRel;
        return nullptr;
    }

    if (hMMR->nBands == 0)
    {
        delete fRel;
        delete hMMR;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to open %s, it has zero usable bands.",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    // Creating the Dataset (with bands or Subdatasets).
    MMRDataset *poDS;
    poDS = new MMRDataset();
    poDS->hMMR =
        hMMR;  // Assigning the information to the Dataset, who will delete that
    poDS->eAccess = poOpenInfo->eAccess;

    // General Dataset information available
    poDS->nRasterXSize = hMMR->nXSize;
    poDS->nRasterYSize = hMMR->nYSize;
    poDS->GetDataSetBoundingBox();  // Fills adfGeoTransform
    poDS->ReadProjection();
    poDS->nBands = 0;

    // Assign every band to a subdataset (if any)
    // If all bands should go to a one single Subdataset, then,
    // no subdataset will be created and all bands will go to this
    // dataset.
    poDS->AssignBandsToSubdataSets();

    // Create subdatasets or add bands, as needed
    if (poDS->nNSubdataSets)
        poDS->CreateSubdatasetsFromBands();
    else
        poDS->AssignBands(poOpenInfo);

    return poDS;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *MMRDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr MMRDataset::SetSpatialRef(const OGRSpatialReference *poSRS)

{
    m_oSRS.Clear();
    if (poSRS)
        m_oSRS = *poSRS;
    bGeoDirty = true;

    return CE_None;
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
/*                          GetColumnsNumberFromREL()                          */
/*                          GetDataSetBoundingBox()                     */
/*                          GetGeoTransform()                           */
/************************************************************************/
int MMRDataset::GetColumnsNumberFromREL(int *nNCols)
{
    // Number of columns of the subdataset (if exist)
    // Section [OVERVIEW:ASPECTES_TECNICS] in rel file
    if (!nNCols || !hMMR || !hMMR->fRel)
        return 1;

    CPLString osValue =
        hMMR->fRel->GetMetadataValue(SECTION_OVVW_ASPECTES_TECNICS, "columns");

    if (osValue.empty())
        return 1;

    *nNCols = atoi(osValue);
    return 0;
}

int MMRDataset::GetRowsNumberFromREL(int *nNRows)
{
    // Number of columns of the subdataset (if exist)
    // Section [OVERVIEW:ASPECTES_TECNICS] in rel file
    // Key raws
    if (!nNRows || !hMMR || !hMMR->fRel)
        return 1;

    CPLString osValue =
        hMMR->fRel->GetMetadataValue(SECTION_OVVW_ASPECTES_TECNICS, "rows");

    if (osValue.empty())
        return 1;

    *nNRows = atoi(osValue);
    return 0;
}

int MMRDataset::GetDataSetBoundingBox()
{
    // Bounding box of the band
    // Section [EXTENT] in rel file

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    if (!hMMR || !hMMR->fRel)
        return 1;

    CPLString osMinX = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MinX");
    if (osMinX.empty())
        return 1;
    adfGeoTransform[0] = atof(osMinX);

    int nNCols;
    if (1 == GetColumnsNumberFromREL(&nNCols) || nNCols <= 0)
        return 1;

    CPLString osMaxX = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MaxX");
    if (osMaxX.empty())
        return 1;

    adfGeoTransform[1] = (atof(osMaxX) - adfGeoTransform[0]) / nNCols;
    adfGeoTransform[2] = 0.0;  // No rotation in MiraMon rasters

    CPLString osMinY = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MinY");
    if (osMinY.empty())
        return 1;

    CPLString osMaxY = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MaxY");
    if (osMaxY.empty())
        return 1;

    int nNRows;
    if (1 == GetRowsNumberFromREL(&nNRows) || nNRows <= 0)
        return 1;

    adfGeoTransform[3] = atof(osMaxY);
    adfGeoTransform[4] = 0.0;

    adfGeoTransform[5] = (atof(osMinY) - adfGeoTransform[3]) / nNRows;

    return 0;
}

int MMRDataset::GetBandBoundingBox(int nIBand)
{
    // Bounding box of the band
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    if (!hMMR || !hMMR->papoBand || nIBand >= hMMR->nBands ||
        !hMMR->papoBand[nIBand])
        return 1;

    MMRBand *poBand = hMMR->papoBand[nIBand];

    adfGeoTransform[0] = poBand->GetBoundingBoxMinX();
    adfGeoTransform[1] =
        (poBand->GetBoundingBoxMaxX() - adfGeoTransform[0]) / poBand->nWidth;
    adfGeoTransform[2] = 0.0;  // No rotation in MiraMon rasters
    adfGeoTransform[3] = poBand->GetBoundingBoxMaxY();
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] =
        (poBand->GetBoundingBoxMinY() - adfGeoTransform[3]) / poBand->nHeight;

    return 0;
}

CPLErr MMRDataset::GetGeoTransform(double *padfTransform)

{
    if (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0 ||
        adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0 ||
        adfGeoTransform[4] != 0.0 || adfGeoTransform[5] != 1.0)
    {
        memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
        return CE_None;
    }

    //return GDALPamDataset::GetGeoTransform(padfTransform);
    return GDALDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr MMRDataset::SetGeoTransform(double *padfTransform)

{
    memcpy(adfGeoTransform, padfTransform, sizeof(double) * 6);
    bGeoDirty = true;

    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int MMRDataset::GetGCPCount()
{
    const int nPAMCount = GDALPamDataset::GetGCPCount();
    return nPAMCount > 0 ? nPAMCount : static_cast<int>(m_aoGCPs.size());
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *MMRDataset::GetGCPSpatialRef() const

{
    const OGRSpatialReference *poSRS = GDALPamDataset::GetGCPSpatialRef();
    if (poSRS)
        return poSRS;
    return !m_aoGCPs.empty() && !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *MMRDataset::GetGCPs()
{
    const GDAL_GCP *psPAMGCPs = GDALPamDataset::GetGCPs();
    if (psPAMGCPs)
        return psPAMGCPs;
    return gdal::GCP::c_ptr(m_aoGCPs);
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/
/*
char **MMRDataset::GetFileList()

{
    CPLStringList oFileList(GDALPamDataset::GetFileList());

    const std::string osIGEFilename = MMRGetIGEFilename(hMMR);
    if (!osIGEFilename.empty())
    {
        oFileList.push_back(osIGEFilename);
    }

    // Request an overview to force opening of dependent overview files.
    //if (nBands > 0 && GetRasterBand(1)->GetOverviewCount() > 0)
    //    GetRasterBand(1)->GetOverview(0);

    if (hMMR->psDependent != nullptr)
    {
        MMRInfo_t *psDep = hMMR->psDependent;

        oFileList.push_back(
            CPLFormFilenameSafe(psDep->pszPath, psDep->pszFileName, nullptr));

        const std::string osIGEFilenameDep = MMRGetIGEFilename(psDep);
        if (!osIGEFilenameDep.empty())
            oFileList.push_back(osIGEFilenameDep);
            
    }

    return oFileList.StealList();
}
*/

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *MMRDataset::Create(const char *pszFilenameIn, int nXSize,
                                int nYSize, int nBandsIn, GDALDataType eType,
                                char **papszParamList)

{
    const int nBits = CSLFetchNameValue(papszParamList, "NBITS") != nullptr
                          ? atoi(CSLFetchNameValue(papszParamList, "NBITS"))
                          : 0;

    const char *pszPixelType = CSLFetchNameValue(papszParamList, "PIXELTYPE");
    if (pszPixelType == nullptr)
        pszPixelType = "";

    // Translate the data type.
    EPTType eMmrDataType;
    switch (eType)
    {
        case GDT_Byte:
            if (nBits == 1)
                eMmrDataType = EPT_u1;
            else if (nBits == 2)
                eMmrDataType = EPT_u2;
            else if (nBits == 4)
                eMmrDataType = EPT_u4;
            else if (EQUAL(pszPixelType, "SIGNEDBYTE"))
                eMmrDataType = EPT_s8;
            else
                eMmrDataType = EPT_u8;
            break;

        case GDT_Int8:
            eMmrDataType = EPT_s8;
            break;

        case GDT_UInt16:
            eMmrDataType = EPT_u16;
            break;

        case GDT_Int16:
            eMmrDataType = EPT_s16;
            break;

        case GDT_Int32:
            eMmrDataType = EPT_s32;
            break;

        case GDT_UInt32:
            eMmrDataType = EPT_u32;
            break;

        case GDT_Float32:
            eMmrDataType = EPT_f32;
            break;

        case GDT_Float64:
            eMmrDataType = EPT_f64;
            break;

        case GDT_CFloat32:
            eMmrDataType = EPT_c64;
            break;

        case GDT_CFloat64:
            eMmrDataType = EPT_c128;
            break;

        default:
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Data type %s not supported by MiraMonRaster format.",
                     GDALGetDataTypeName(eType));
            return nullptr;
    }

    const bool bForceToPEString =
        CPLFetchBool(papszParamList, "FORCETOPESTRING", false);
    const bool bDisablePEString =
        CPLFetchBool(papszParamList, "DISABLEPESTRING", false);
    if (bForceToPEString && bDisablePEString)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FORCETOPESTRING and DISABLEPESTRING are mutually exclusive");
        return nullptr;
    }

    // Create the new file.
    MMRHandle hMMR = MMRCreate(pszFilenameIn, nXSize, nYSize, nBandsIn,
                               eMmrDataType, papszParamList);
    if (hMMR == nullptr)
        return nullptr;

    if (MMRClose(hMMR) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        return nullptr;
    }

    // Open the dataset normally.
    MMRDataset *poDS = (MMRDataset *)GDALOpen(pszFilenameIn, GA_Update);

    // Special creation option to disable checking for UTM
    // parameters when writing the projection.  This is a special
    // hack for sam.gillingham@nrm.qld.gov.au.
    if (poDS != nullptr)
    {
        poDS->bIgnoreUTM = CPLFetchBool(papszParamList, "IGNOREUTM", false);
    }

    // Sometimes we can improve ArcGIS compatibility by forcing
    // generation of a PEString instead of traditional Imagine
    // coordinate system descriptions.
    if (poDS != nullptr)
    {
        poDS->bForceToPEString = bForceToPEString;
        poDS->bDisablePEString = bDisablePEString;
    }

    return poDS;
}

/************************************************************************/
/*                             CopyFiles()                              */
/*                                                                      */
/*      Custom CopyFiles() implementation that knows how to update      */
/*      filename references in .img and .aux files.                     */
/************************************************************************/

CPLErr MMRDataset::CopyFiles(const char *pszNewName, const char *pszOldName)

{
    // Rename all the files at the filesystem level.
    CPLErr eErr = GDALDriver::DefaultCopyFiles(pszNewName, pszOldName);

    if (eErr != CE_None)
        return eErr;

    // Now try to go into the .img file and update RRDNames[] lists.
    CPLString osOldBasename = CPLGetBasenameSafe(pszOldName);
    CPLString osNewBasename = CPLGetBasenameSafe(pszNewName);

    if (osOldBasename != osNewBasename)
    {
        MMRRel *fRel = new MMRRel(pszNewName);  // ·$·TODO Alliberar
        MMRHandle hMMR = fRel->GetInfoFromREL(pszNewName, "r+");

        if (hMMR != nullptr)
        {
            eErr = MMRRenameReferences(hMMR, osNewBasename, osOldBasename);

            if (MMRClose(hMMR) != 0)
                eErr = CE_Failure;
        }
    }

    return eErr;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *MMRDataset::CreateCopy(const char *pszFileName,
                                    GDALDataset *poSrcDS, int /* bStrict */,
                                    char **papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData)
{
    // Do we really just want to create an .aux file?
    const bool bCreateAux = CPLFetchBool(papszOptions, "AUX", false);

    // Establish a representative data type to use.
    char **papszModOptions = CSLDuplicate(papszOptions);
    if (!pfnProgress(0.0, nullptr, pProgressData))
    {
        CSLDestroy(papszModOptions);
        return nullptr;
    }

    const int nBandCount = poSrcDS->GetRasterCount();
    GDALDataType eType = GDT_Unknown;

    for (int iBand = 0; iBand < nBandCount; iBand++)
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand(iBand + 1);
        if (iBand == 0)
            eType = poBand->GetRasterDataType();
        else
            eType = GDALDataTypeUnion(eType, poBand->GetRasterDataType());
    }

    // If we have PIXELTYPE metadata in the source, pass it
    // through as a creation option.
    if (CSLFetchNameValue(papszOptions, "PIXELTYPE") == nullptr &&
        nBandCount > 0 && eType == GDT_Byte)
    {
        auto poSrcBand = poSrcDS->GetRasterBand(1);
        poSrcBand->EnablePixelTypeSignedByteWarning(false);
        const char *pszPixelType =
            poSrcBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        poSrcBand->EnablePixelTypeSignedByteWarning(true);
        if (pszPixelType)
        {
            papszModOptions =
                CSLSetNameValue(papszModOptions, "PIXELTYPE", pszPixelType);
        }
    }

    MMRDataset *poDS = (MMRDataset *)Create(
        pszFileName, poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
        nBandCount, eType, papszModOptions);

    CSLDestroy(papszModOptions);

    if (poDS == nullptr)
        return nullptr;

    // Does the source have a PCT or RAT for any of the bands?  If so, copy it
    // over.
    for (int iBand = 0; iBand < nBandCount; iBand++)
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand(iBand + 1);

        GDALColorTable *poCT = poBand->GetColorTable();
        if (poCT != nullptr)
        {
            poDS->GetRasterBand(iBand + 1)->SetColorTable(poCT);
        }

        if (poBand->GetDefaultRAT() != nullptr)
            poDS->GetRasterBand(iBand + 1)->SetDefaultRAT(
                poBand->GetDefaultRAT());
    }

    // Do we have metadata for any of the bands or the dataset as a whole?
    if (poSrcDS->GetMetadata() != nullptr)
        poDS->SetMetadata(poSrcDS->GetMetadata());

    for (int iBand = 0; iBand < nBandCount; iBand++)
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand + 1);
        GDALRasterBand *poDstBand = poDS->GetRasterBand(iBand + 1);

        if (poSrcBand->GetMetadata() != nullptr)
            poDstBand->SetMetadata(poSrcBand->GetMetadata());

        if (strlen(poSrcBand->GetDescription()) > 0)
            poDstBand->SetDescription(poSrcBand->GetDescription());

        int bSuccess = FALSE;
        const double dfNoDataValue = poSrcBand->GetNoDataValue(&bSuccess);
        if (bSuccess)
            poDstBand->SetNoDataValue(dfNoDataValue);
    }

    // Copy projection information.
    double adfGeoTransform[6] = {};

    if (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None)
        poDS->SetGeoTransform(adfGeoTransform);

    const char *pszProj = poSrcDS->GetProjectionRef();
    if (pszProj != nullptr && strlen(pszProj) > 0)
        poDS->SetProjection(pszProj);

    // Copy the imagery.
    if (!bCreateAux)
    {
        const CPLErr eErr = GDALDatasetCopyWholeRaster(
            (GDALDatasetH)poSrcDS, (GDALDatasetH)poDS, nullptr, pfnProgress,
            pProgressData);

        if (eErr != CE_None)
        {
            delete poDS;
            return nullptr;
        }
    }

    // Do we want to generate statistics and a histogram?
    if (CPLFetchBool(papszOptions, "STATISTICS", false))
    {
        for (int iBand = 0; iBand < nBandCount; iBand++)
        {
            GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand + 1);
            double dfMin = 0.0;
            double dfMax = 0.0;
            double dfMean = 0.0;
            double dfStdDev = 0.0;
            char **papszStatsMD = nullptr;

            // Statistics
            if (poSrcBand->GetStatistics(TRUE, FALSE, &dfMin, &dfMax, &dfMean,
                                         &dfStdDev) == CE_None ||
                poSrcBand->ComputeStatistics(TRUE, &dfMin, &dfMax, &dfMean,
                                             &dfStdDev, pfnProgress,
                                             pProgressData) == CE_None)
            {
                CPLString osValue;

                papszStatsMD =
                    CSLSetNameValue(papszStatsMD, "STATISTICS_MINIMUM",
                                    osValue.Printf("%.15g", dfMin));
                papszStatsMD =
                    CSLSetNameValue(papszStatsMD, "STATISTICS_MAXIMUM",
                                    osValue.Printf("%.15g", dfMax));
                papszStatsMD = CSLSetNameValue(papszStatsMD, "STATISTICS_MEAN",
                                               osValue.Printf("%.15g", dfMean));
                papszStatsMD =
                    CSLSetNameValue(papszStatsMD, "STATISTICS_STDDEV",
                                    osValue.Printf("%.15g", dfStdDev));
            }

            // Histogram
            int nBuckets = 0;
            GUIntBig *panHistogram = nullptr;

            if (poSrcBand->GetDefaultHistogram(&dfMin, &dfMax, &nBuckets,
                                               &panHistogram, TRUE, pfnProgress,
                                               pProgressData) == CE_None)
            {
                CPLString osValue;
                const double dfBinWidth = (dfMax - dfMin) / nBuckets;

                papszStatsMD = CSLSetNameValue(
                    papszStatsMD, "STATISTICS_HISTOMIN",
                    osValue.Printf("%.15g", dfMin + dfBinWidth * 0.5));
                papszStatsMD = CSLSetNameValue(
                    papszStatsMD, "STATISTICS_HISTOMAX",
                    osValue.Printf("%.15g", dfMax - dfBinWidth * 0.5));
                papszStatsMD =
                    CSLSetNameValue(papszStatsMD, "STATISTICS_HISTONUMBINS",
                                    osValue.Printf("%d", nBuckets));

                int nBinValuesLen = 0;
                char *pszBinValues =
                    static_cast<char *>(CPLCalloc(20, nBuckets + 1));
                for (int iBin = 0; iBin < nBuckets; iBin++)
                {

                    strcat(pszBinValues + nBinValuesLen,
                           osValue.Printf(CPL_FRMT_GUIB, panHistogram[iBin]));
                    strcat(pszBinValues + nBinValuesLen, "|");
                    nBinValuesLen +=
                        static_cast<int>(strlen(pszBinValues + nBinValuesLen));
                }
                papszStatsMD = CSLSetNameValue(
                    papszStatsMD, "STATISTICS_HISTOBINVALUES", pszBinValues);
                CPLFree(pszBinValues);
            }

            CPLFree(panHistogram);

            if (CSLCount(papszStatsMD) > 0)
                MMRSetMetadata(poDS->hMMR, iBand + 1, papszStatsMD);

            CSLDestroy(papszStatsMD);
        }
    }

    // All report completion.
    if (!pfnProgress(1.0, nullptr, pProgressData))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        delete poDS;

        GDALDriver *poMMRDriver =
            (GDALDriver *)GDALGetDriverByName("MiraMonRaster");
        poMMRDriver->Delete(pszFileName);
        return nullptr;
    }

    poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);

    return poDS;
}

/************************************************************************/
/*                GDALRegister_MiraMonRaster()                          */
/************************************************************************/

void GDALRegister_MiraMonRaster()

{
    if (GDALGetDriverByName("MiraMonRaster") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    //·$·TODO Fitxer ajuda!!
    poDriver->SetDescription("MiraMonRaster");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "MiraMon Raster Images (.img)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/hfa.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "img");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONDATATYPES,
        "Byte Int8 Int16 UInt16 Int32 UInt32 Float32 Float64 "
        "CFloat32 CFloat64");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='BLOCKSIZE' type='integer' description='tile "
        "width/height (32-2048)' default='64'/>"
        "   <Option name='USE_SPILL' type='boolean' description='Force use of "
        "spill file'/>"
        "   <Option name='COMPRESSED' alias='COMPRESS' type='boolean' "
        "description='compress blocks'/>"
        "   <Option name='PIXELTYPE' type='string' description='(deprecated, "
        "use Int8) By setting this to SIGNEDBYTE, a new Byte file can be "
        "forced to be written as signed byte'/>"
        "   <Option name='AUX' type='boolean' description='Create an .aux "
        "file'/>"
        "   <Option name='IGNOREUTM' type='boolean' description='Ignore UTM "
        "when selecting coordinate system - will use Transverse Mercator. Only "
        "used for Create() method'/>"
        "   <Option name='NBITS' type='integer' description='Create file with "
        "special sub-byte data type (1/2/4)'/>"
        "   <Option name='STATISTICS' type='boolean' description='Generate "
        "statistics and a histogram'/>"
        "   <Option name='DEPENDENT_FILE' type='string' description='Name of "
        "dependent file (must not have absolute path)'/>"
        "   <Option name='FORCETOPESTRING' type='boolean' description='Force "
        "use of ArcGIS PE String in file instead of Imagine coordinate system "
        "format' default='NO'/>"
        "   <Option name='DISABLEPESTRING' type='boolean' description='Disable "
        "use of ArcGIS PE String' default='NO'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    //poDriver->SetMetadataItem(GDAL_DCAP_UPDATE, "YES");
    //poDriver->SetMetadataItem(GDAL_DMD_UPDATE_ITEMS,
    //                          "GeoTransform SRS NoData "
    //                          "RasterValues "
    //                          "DatasetMetadata BandMetadata");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnOpen = MMRDataset::Open;
    poDriver->pfnCreate = MMRDataset::Create;
    poDriver->pfnCreateCopy = MMRDataset::CreateCopy;
    poDriver->pfnIdentify = MMRDataset::Identify;
    poDriver->pfnCopyFiles = MMRDataset::CopyFiles;

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

void MMRDataset::AssignBands(GDALOpenInfo *poOpenInfo)
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

        /*char **papszMD = MMRGetMetadata(hMMR, i + 1);
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
    char **papszMD = MMRGetMetadata(hMMR, 0);
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
        const char *pszEU = MMRReadElevationUnit(hMMR, iBand);

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

    // Initialize any PAM information.
    SetDescription(poOpenInfo->pszFilename);
    //TryLoadXML();

    // Clear dirty metadata flags.
    for (int i = 0; i < nBands; i++)
    {
        MMRRasterBand *poBand =
            static_cast<MMRRasterBand *>(GetRasterBand(i + 1));
        poBand->bMetadataDirty = false;
    }
    bMetadataDirty = false;
}
