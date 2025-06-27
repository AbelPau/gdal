/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements OGRMiraMonDataSource class.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************
 *
 * miramonopen.cpp
 *
 * Supporting routines for reading Erdas Imagine (.imf) Hierarchical
 * File Architecture files.  This is intended to be a library independent
 * of the GDAL core, but dependent on the Common Portability Library.
 *
 */

#include "cpl_port.h"
#include "miramon_p.h"

#include <cerrno>
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
#include <memory>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_priv.h"
#include "miramon.h"
#include "ogr_proj_p.h"
#include "proj.h"

#ifdef MSVC
#include "..\frmts\miramon_common\mm_gdal_functions.h"
//#include "..\frmts\miramon_common\mm_gdal_constants.h"
#else
#include "../frmts/miramon_common/mm_gdal_functions.h"
//#include "../frmts/miramon_common/mm_gdal_constants.h"
#endif

#include "miramon_rastertools.h"

static const char *const apszAuxMetadataItems[] = {
    // node/entry            field_name                  metadata_key       type
    "Statistics",
    "dminimum",
    "STATISTICS_MINIMUM",
    "Esta_Statistics",
    "Statistics",
    "dmaximum",
    "STATISTICS_MAXIMUM",
    "Esta_Statistics",
    "Statistics",
    "dmean",
    "STATISTICS_MEAN",
    "Esta_Statistics",
    "Statistics",
    "dmedian",
    "STATISTICS_MEDIAN",
    "Esta_Statistics",
    "Statistics",
    "dmode",
    "STATISTICS_MODE",
    "Esta_Statistics",
    "Statistics",
    "dstddev",
    "STATISTICS_STDDEV",
    "Esta_Statistics",
    "HistogramParameters",
    "lBinFunction.numBins",
    "STATISTICS_HISTONUMBINS",
    "Eimg_StatisticsParameters830",
    "HistogramParameters",
    "dBinFunction.minLimit",
    "STATISTICS_HISTOMIN",
    "Eimg_StatisticsParameters830",
    "HistogramParameters",
    "dBinFunction.maxLimit",
    "STATISTICS_HISTOMAX",
    "Eimg_StatisticsParameters830",
    "StatisticsParameters",
    "lSkipFactorX",
    "STATISTICS_SKIPFACTORX",
    "",
    "StatisticsParameters",
    "lSkipFactorY",
    "STATISTICS_SKIPFACTORY",
    "",
    "StatisticsParameters",
    "dExcludedValues",
    "STATISTICS_EXCLUDEDVALUES",
    "",
    "",
    "elayerType",
    "LAYER_TYPE",
    "",
    "RRDInfoList",
    "salgorithm.string",
    "OVERVIEWS_ALGORITHM",
    "Emif_String",
    nullptr};

const char *const *GetMMRAuxMetaDataList()
{
    return apszAuxMetadataItems;
}

/************************************************************************/
/*                              MMRClose()                              */
/************************************************************************/

int MMRClose(MMRHandle hMMR)

{
    int nRet = 0;

    for (int i = 0; i < hMMR->nBands; i++)
    {
        delete hMMR->papoBand[i];
    }
    delete hMMR->papoBand;

    delete hMMR->fRel;
    hMMR->fRel = nullptr;

    delete hMMR;
    return nRet;
}

/************************************************************************/
/*                           MMRGetBandInfo()                           */
/************************************************************************/

CPLErr MMRGetBandInfo(MMRHandle hMMR, int nBand, CPLString *osBandSection,
                      MMDataType *eMMRDataType,
                      MMBytesPerPixel *eMMBytesPerPixel, int *pnBlockXSize,
                      int *pnBlockYSize, int *pnCompressionType)

{
    if (nBand < 0 || nBand > hMMR->nBands)
    {
        CPLAssert(false);
        return CE_Failure;
    }

    MMRBand *poBand = hMMR->papoBand[nBand - 1];

    if (osBandSection != nullptr)
        *osBandSection = poBand->GetBandSection();

    if (eMMRDataType != nullptr)
        *eMMRDataType = poBand->GeteMMDataType();

    if (eMMBytesPerPixel != nullptr)
        *eMMBytesPerPixel = poBand->GeteMMBytesPerPixel();

    if (pnBlockXSize != nullptr)
        *pnBlockXSize = poBand->nBlockXSize;

    if (pnBlockYSize != nullptr)
        *pnBlockYSize = poBand->nBlockYSize;

    // Get compression code from RasterDMS.
    if (pnCompressionType != nullptr)
    {
        *pnCompressionType = 0;

        // ·$·TODO NO se si cal MMREntry *poDMS = poBand->poNode->GetNamedChild("RasterDMS");

        // ·$·TODO NO se si cal if (poDMS != nullptr)
        // ·$·TODO NO se si cal *pnCompressionType = poDMS->GetIntField("compressionType");
    }

    return CE_None;
}

/************************************************************************/
/*                          MMRGetBandNoData()                          */
/*                                                                      */
/*      returns true if value is set, otherwise false.                  */
/************************************************************************/

int MMRGetBandNoData(MMRHandle hMMR, int nBand, double *pdfNoData)

{
    if (nBand < 0 || (nBand - 1) > hMMR->nBands || !pdfNoData)
    {
        CPLAssert(false);
        return false;
    }

    MMRBand *poBand = hMMR->papoBand[nBand - 1];
    if (!poBand)
        return false;

    if (!poBand->bNoDataSet)
        return false;

    *pdfNoData = poBand->dfNoData;
    return poBand->bNoDataSet;
}

/************************************************************************/
/*                         MMRGetRasterBlock()                          */
/************************************************************************/

CPLErr MMRGetRasterBlock(MMRHandle hMMR, int nBand, int nXBlock, int nYBlock,
                         void *pData)

{
    return MMRGetRasterBlockEx(hMMR, nBand, nXBlock, nYBlock, pData, -1);
}

/************************************************************************/
/*                        MMRGetRasterBlockEx()                         */
/************************************************************************/

CPLErr MMRGetRasterBlockEx(MMRHandle hMMR, int nBand, int nXBlock, int nYBlock,
                           void *pData, int nDataSize)

{
    if (nBand < 1 || nBand > hMMR->nBands)
        return CE_Failure;

    return hMMR->papoBand[nBand - 1]->GetRasterBlock(nXBlock, nYBlock, pData,
                                                     nDataSize);
}

/************************************************************************/
/*                         MMRGetDataTypeBits()                         */
/************************************************************************/

int MMRGetDataTypeBits(EPTType eDataType)

{
    switch (eDataType)
    {
        case EPT_u1:
            return 1;

        case EPT_u2:
            return 2;

        case EPT_u4:
            return 4;

        case EPT_u8:
        case EPT_s8:
            return 8;

        case EPT_u16:
        case EPT_s16:
            return 16;

        case EPT_u32:
        case EPT_s32:
        case EPT_f32:
            return 32;

        case EPT_f64:
        case EPT_c64:
            return 64;

        case EPT_c128:
            return 128;
    }

    CPLAssert(false);
    return 1;
}

/************************************************************************/
/*                         MMRGetDataTypeName()                         */
/************************************************************************/

const char *MMRGetDataTypeName(EPTType eDataType)

{
    switch (eDataType)
    {
        case EPT_u1:
            return "u1";

        case EPT_u2:
            return "u2";

        case EPT_u4:
            return "u4";

        case EPT_u8:
            return "u8";

        case EPT_s8:
            return "s8";

        case EPT_u16:
            return "u16";

        case EPT_s16:
            return "s16";

        case EPT_u32:
            return "u32";

        case EPT_s32:
            return "s32";

        case EPT_f32:
            return "f32";

        case EPT_f64:
            return "f64";

        case EPT_c64:
            return "c64";

        case EPT_c128:
            return "c128";

        default:
            CPLAssert(false);
            return "unknown";
    }
}

/************************************************************************/
/*                             MMRGetPCT()                              */
/*                                                                      */
/*      Read the PCT from a band, if it has one.                        */
/************************************************************************/

CPLErr MMRGetPCT(MMRHandle hMMR, int nBand)

{
    if (nBand < 1 || nBand > hMMR->nBands)
        return CE_Failure;

    return hMMR->papoBand[nBand - 1]->GetPCT();
}

/************************************************************************/
/*                            MMRStandard()                             */
/*                                                                      */
/*      Swap byte order on MSB systems.                                 */
/************************************************************************/

#ifdef CPL_MSB
void MMRStandard(int nBytes, void *pData)

{
    GByte *pabyData = static_cast<GByte *>(pData);

    for (int i = nBytes / 2 - 1; i >= 0; i--)
    {
        GByte byTemp = pabyData[i];
        pabyData[i] = pabyData[nBytes - i - 1];
        pabyData[nBytes - i - 1] = byTemp;
    }
}
#endif
