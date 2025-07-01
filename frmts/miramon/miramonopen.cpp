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
 *****************************************************************************/

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

/************************************************************************/
/*                              MMRClose()                              */
/************************************************************************/

int MMRClose(MMRInfo *hMMR)

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

CPLErr MMRGetBandInfo(const MMRInfo &hMMR, int nBand, CPLString *osBandSection,
                      MMDataType *eMMRDataType,
                      MMBytesPerPixel *eMMBytesPerPixel, int *pnBlockXSize,
                      int *pnBlockYSize)

{
    if (nBand < 0 || nBand > hMMR.nBands)
    {
        CPLAssert(false);
        return CE_Failure;
    }

    MMRBand *poBand = hMMR.papoBand[nBand - 1];

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

    return CE_None;
}

/************************************************************************/
/*                          MMRGetBandNoData()                          */
/*                                                                      */
/*      returns true if value is set, otherwise false.                  */
/************************************************************************/

int MMRGetBandNoData(MMRInfo &hMMR, int nBand, double *pdfNoData)

{
    if (nBand < 0 || (nBand - 1) > hMMR.nBands || !pdfNoData)
    {
        CPLAssert(false);
        return false;
    }

    MMRBand *poBand = hMMR.papoBand[nBand - 1];
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

CPLErr MMRGetRasterBlock(MMRInfo &hMMR, int nBand, int nXBlock, int nYBlock,
                         void *pData)

{
    return MMRGetRasterBlockEx(hMMR, nBand, nXBlock, nYBlock, pData, -1);
}

/************************************************************************/
/*                        MMRGetRasterBlockEx()                         */
/************************************************************************/

CPLErr MMRGetRasterBlockEx(MMRInfo &hMMR, int nBand, int nXBlock, int nYBlock,
                           void *pData, int nDataSize)

{
    if (nBand < 1 || nBand > hMMR.nBands)
        return CE_Failure;

    return hMMR.papoBand[nBand - 1]->GetRasterBlock(nXBlock, nYBlock, pData,
                                                    nDataSize);
}

/************************************************************************/
/*                             MMRGetPCT()                              */
/*                                                                      */
/*      Read the PCT from a band, if it has one.                        */
/************************************************************************/

CPLErr MMRGetPCT(MMRInfo &hMMR, int nBand)

{
    if (nBand < 1 || nBand > hMMR.nBands)
        return CE_Failure;

    return hMMR.papoBand[nBand - 1]->GetPCT();
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
