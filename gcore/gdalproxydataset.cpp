/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  A dataset and raster band classes that act as proxy for underlying
 *           GDALDataset* and GDALRasterBand*
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_proxy.h"

#include <cstddef>

#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_virtualmem.h"
#include "gdal.h"
#include "gdal_priv.h"

/*! @cond Doxygen_Suppress */
/* ******************************************************************** */
/*                        GDALProxyDataset                              */
/* ******************************************************************** */

#define D_PROXY_METHOD_WITH_RET(retType, retErrValue, methodName, argList,     \
                                argParams)                                     \
    retType GDALProxyDataset::methodName argList                               \
    {                                                                          \
        retType ret;                                                           \
        GDALDataset *poUnderlyingDataset = RefUnderlyingDataset();             \
        if (poUnderlyingDataset)                                               \
        {                                                                      \
            ret = poUnderlyingDataset->methodName argParams;                   \
            UnrefUnderlyingDataset(poUnderlyingDataset);                       \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            ret = retErrValue;                                                 \
        }                                                                      \
        return ret;                                                            \
    }

CPLErr GDALProxyDataset::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
    GSpacing nLineSpace, GSpacing nBandSpace, GDALRasterIOExtraArg *psExtraArg)
{
    CPLErr ret;
    GDALDataset *poUnderlyingDataset = RefUnderlyingDataset();
    if (poUnderlyingDataset)
    {
        /* --------------------------------------------------------------------
         */
        /*      Do some validation of parameters. */
        /* --------------------------------------------------------------------
         */
        if (nXOff + nXSize > poUnderlyingDataset->GetRasterXSize() ||
            nYOff + nYSize > poUnderlyingDataset->GetRasterYSize())
        {
            ReportError(CE_Failure, CPLE_IllegalArg,
                        "Access window out of range in RasterIO().  Requested\n"
                        "(%d,%d) of size %dx%d on raster of %dx%d.",
                        nXOff, nYOff, nXSize, nYSize,
                        poUnderlyingDataset->GetRasterXSize(),
                        poUnderlyingDataset->GetRasterYSize());
            ret = CE_Failure;
        }
        else if (panBandMap == nullptr &&
                 nBandCount > poUnderlyingDataset->GetRasterCount())
        {
            ReportError(CE_Failure, CPLE_IllegalArg,
                        "%s: nBandCount cannot be greater than %d", "IRasterIO",
                        poUnderlyingDataset->GetRasterCount());
            ret = CE_Failure;
        }
        else
        {
            ret = CE_None;
            for (int i = 0; i < nBandCount && ret == CE_None; ++i)
            {
                int iBand = (panBandMap != nullptr) ? panBandMap[i] : i + 1;
                if (iBand < 1 || iBand > poUnderlyingDataset->GetRasterCount())
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "%s: panBandMap[%d] = %d, this band does not "
                                "exist on dataset.",
                                "IRasterIO", i, iBand);
                    ret = CE_Failure;
                }

                if (ret == CE_None &&
                    poUnderlyingDataset->GetRasterBand(iBand) == nullptr)
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "%s: panBandMap[%d]=%d, this band should exist "
                                "but is NULL!",
                                "IRasterIO", i, iBand);
                    ret = CE_Failure;
                }
            }
            if (ret != CE_Failure)
            {
                ret = poUnderlyingDataset->IRasterIO(
                    eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize,
                    nBufYSize, eBufType, nBandCount, panBandMap, nPixelSpace,
                    nLineSpace, nBandSpace, psExtraArg);
            }
        }
        UnrefUnderlyingDataset(poUnderlyingDataset);
    }
    else
    {
        ret = CE_Failure;
    }
    return ret;
}

D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, BlockBasedRasterIO,
                        (GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                         int nYSize, void *pData, int nBufXSize, int nBufYSize,
                         GDALDataType eBufType, int nBandCount,
                         const int *panBandMap, GSpacing nPixelSpace,
                         GSpacing nLineSpace, GSpacing nBandSpace,
                         GDALRasterIOExtraArg *psExtraArg),
                        (eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                         nBufXSize, nBufYSize, eBufType, nBandCount, panBandMap,
                         nPixelSpace, nLineSpace, nBandSpace, psExtraArg))

D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, IBuildOverviews,
                        (const char *pszResampling, int nOverviews,
                         const int *panOverviewList, int nListBands,
                         const int *panBandList, GDALProgressFunc pfnProgress,
                         void *pProgressData, CSLConstList papszOptions),
                        (pszResampling, nOverviews, panOverviewList, nListBands,
                         panBandList, pfnProgress, pProgressData, papszOptions))

D_PROXY_METHOD_WITH_RET(CPLStringList, CPLStringList(), GetCompressionFormats,
                        (int nXOff, int nYOff, int nXSize, int nYSize,
                         int nBandCount, const int *panBandList),
                        (nXOff, nYOff, nXSize, nYSize, nBandCount, panBandList))

D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, ReadCompressedData,
                        (const char *pszFormat, int nXOff, int nYOff,
                         int nXSize, int nYSize, int nBandCount,
                         const int *panBandList, void **ppBuffer,
                         size_t *pnBufferSize, char **ppszDetailedFormat),
                        (pszFormat, nXOff, nYOff, nXSize, nYSize, nBandCount,
                         panBandList, ppBuffer, pnBufferSize,
                         ppszDetailedFormat))

D_PROXY_METHOD_WITH_RET(CPLErr, CE_None, FlushCache, (bool bAtClosing),
                        (bAtClosing))

D_PROXY_METHOD_WITH_RET(char **, nullptr, GetMetadataDomainList, (), ())
D_PROXY_METHOD_WITH_RET(char **, nullptr, GetMetadata, (const char *pszDomain),
                        (pszDomain))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetMetadata,
                        (char **papszMetadata, const char *pszDomain),
                        (papszMetadata, pszDomain))
D_PROXY_METHOD_WITH_RET(const char *, nullptr, GetMetadataItem,
                        (const char *pszName, const char *pszDomain),
                        (pszName, pszDomain))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetMetadataItem,
                        (const char *pszName, const char *pszValue,
                         const char *pszDomain),
                        (pszName, pszValue, pszDomain))

D_PROXY_METHOD_WITH_RET(const OGRSpatialReference *, nullptr, GetSpatialRef,
                        () const, ())
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetSpatialRef,
                        (const OGRSpatialReference *poSRS), (poSRS))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, GetGeoTransform,
                        (GDALGeoTransform & gt) const, (gt))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetGeoTransform,
                        (const GDALGeoTransform &gt), (gt))

D_PROXY_METHOD_WITH_RET(void *, nullptr, GetInternalHandle, (const char *arg1),
                        (arg1))
D_PROXY_METHOD_WITH_RET(GDALDriver *, nullptr, GetDriver, (), ())
D_PROXY_METHOD_WITH_RET(char **, nullptr, GetFileList, (), ())
D_PROXY_METHOD_WITH_RET(int, 0, GetGCPCount, (), ())
D_PROXY_METHOD_WITH_RET(const OGRSpatialReference *, nullptr, GetGCPSpatialRef,
                        () const, ())
D_PROXY_METHOD_WITH_RET(const GDAL_GCP *, nullptr, GetGCPs, (), ())
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetGCPs,
                        (int nGCPCount, const GDAL_GCP *pasGCPList,
                         const OGRSpatialReference *poGCP_SRS),
                        (nGCPCount, pasGCPList, poGCP_SRS))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, AdviseRead,
                        (int nXOff, int nYOff, int nXSize, int nYSize,
                         int nBufXSize, int nBufYSize, GDALDataType eDT,
                         int nBandCount, int *panBandList, char **papszOptions),
                        (nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                         eDT, nBandCount, panBandList, papszOptions))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, CreateMaskBand, (int nFlagsIn),
                        (nFlagsIn))

/************************************************************************/
/*                    UnrefUnderlyingDataset()                        */
/************************************************************************/

void GDALProxyDataset::UnrefUnderlyingDataset(
    GDALDataset * /* poUnderlyingDataset */) const
{
}

/* ******************************************************************** */
/*                        GDALProxyRasterBand                           */
/* ******************************************************************** */

#define RB_PROXY_METHOD_WITH_RET(retType, retErrValue, methodName, argList,    \
                                 argParams)                                    \
    retType GDALProxyRasterBand::methodName argList                            \
    {                                                                          \
        retType ret;                                                           \
        GDALRasterBand *poSrcBand = RefUnderlyingRasterBand();                 \
        if (poSrcBand)                                                         \
        {                                                                      \
            ret = poSrcBand->methodName argParams;                             \
            UnrefUnderlyingRasterBand(poSrcBand);                              \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            ret = retErrValue;                                                 \
        }                                                                      \
        return ret;                                                            \
    }

#define RB_PROXY_METHOD_WITH_RET_WITH_INIT_BLOCK(                              \
    retType, retErrValue, methodName, argList, argParams)                      \
    retType GDALProxyRasterBand::methodName argList                            \
    {                                                                          \
        retType ret;                                                           \
        GDALRasterBand *poSrcBand = RefUnderlyingRasterBand();                 \
        if (poSrcBand)                                                         \
        {                                                                      \
            if (!poSrcBand->InitBlockInfo())                                   \
                ret = CE_Failure;                                              \
            else                                                               \
            {                                                                  \
                int nSrcBlockXSize, nSrcBlockYSize;                            \
                poSrcBand->GetBlockSize(&nSrcBlockXSize, &nSrcBlockYSize);     \
                if (poSrcBand->GetRasterDataType() != GetRasterDataType())     \
                {                                                              \
                    CPLError(                                                  \
                        CE_Failure, CPLE_AppDefined,                           \
                        "Inconsistent datatype between proxy and source");     \
                    ret = CE_Failure;                                          \
                }                                                              \
                else if (nSrcBlockXSize != nBlockXSize ||                      \
                         nSrcBlockYSize != nBlockYSize)                        \
                {                                                              \
                    CPLError(CE_Failure, CPLE_AppDefined,                      \
                             "Inconsistent block dimensions between proxy "    \
                             "and source");                                    \
                    ret = CE_Failure;                                          \
                }                                                              \
                else                                                           \
                {                                                              \
                    ret = poSrcBand->methodName argParams;                     \
                }                                                              \
            }                                                                  \
            UnrefUnderlyingRasterBand(poSrcBand);                              \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            ret = retErrValue;                                                 \
        }                                                                      \
        return ret;                                                            \
    }

RB_PROXY_METHOD_WITH_RET_WITH_INIT_BLOCK(CPLErr, CE_Failure, IReadBlock,
                                         (int nXBlockOff, int nYBlockOff,
                                          void *pImage),
                                         (nXBlockOff, nYBlockOff, pImage))
RB_PROXY_METHOD_WITH_RET_WITH_INIT_BLOCK(CPLErr, CE_Failure, IWriteBlock,
                                         (int nXBlockOff, int nYBlockOff,
                                          void *pImage),
                                         (nXBlockOff, nYBlockOff, pImage))

CPLErr GDALProxyRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                      int nXSize, int nYSize, void *pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace, GSpacing nLineSpace,
                                      GDALRasterIOExtraArg *psExtraArg)
{
    CPLErr ret;
    GDALRasterBand *poSrcBand = RefUnderlyingRasterBand();
    if (poSrcBand)
    {
        /* --------------------------------------------------------------------
         */
        /*      Do some validation of parameters. */
        /* --------------------------------------------------------------------
         */
        if (nXOff + nXSize > poSrcBand->GetXSize() ||
            nYOff + nYSize > poSrcBand->GetYSize())
        {
            ReportError(CE_Failure, CPLE_IllegalArg,
                        "Access window out of range in RasterIO().  Requested\n"
                        "(%d,%d) of size %dx%d on raster of %dx%d.",
                        nXOff, nYOff, nXSize, nYSize, poSrcBand->GetXSize(),
                        poSrcBand->GetYSize());
            ret = CE_Failure;
        }
        else
        {
            ret = poSrcBand->IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                       pData, nBufXSize, nBufYSize, eBufType,
                                       nPixelSpace, nLineSpace, psExtraArg);
        }
        UnrefUnderlyingRasterBand(poSrcBand);
    }
    else
    {
        ret = CE_Failure;
    }
    return ret;
}

int GDALProxyRasterBand::IGetDataCoverageStatus(int nXOff, int nYOff,
                                                int nXSize, int nYSize,
                                                int nMaskFlagStop,
                                                double *pdfDataPct)
{
    if (pdfDataPct)
        *pdfDataPct = 0.0;
    int ret = GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
              GDAL_DATA_COVERAGE_STATUS_EMPTY;
    GDALRasterBand *poSrcBand = RefUnderlyingRasterBand();
    if (poSrcBand)
    {
        ret = poSrcBand->GetDataCoverageStatus(nXOff, nYOff, nXSize, nYSize,
                                               nMaskFlagStop, pdfDataPct);
        UnrefUnderlyingRasterBand(poSrcBand);
    }
    return ret;
}

RB_PROXY_METHOD_WITH_RET(char **, nullptr, GetMetadataDomainList, (), ())
RB_PROXY_METHOD_WITH_RET(char **, nullptr, GetMetadata, (const char *pszDomain),
                         (pszDomain))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetMetadata,
                         (char **papszMetadata, const char *pszDomain),
                         (papszMetadata, pszDomain))

const char *GDALProxyRasterBand::GetMetadataItem(const char *pszKey,
                                                 const char *pszDomain)
{
    const char *pszRet = nullptr;
    GDALRasterBand *poSrcBand = RefUnderlyingRasterBand();
    if (poSrcBand)
    {
        if (!m_bEnablePixelTypeSignedByteWarning)
            poSrcBand->EnablePixelTypeSignedByteWarning(false);
        pszRet = poSrcBand->GetMetadataItem(pszKey, pszDomain);
        poSrcBand->EnablePixelTypeSignedByteWarning(true);
        UnrefUnderlyingRasterBand(poSrcBand);
    }
    return pszRet;
}

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetMetadataItem,
                         (const char *pszName, const char *pszValue,
                          const char *pszDomain),
                         (pszName, pszValue, pszDomain))

RB_PROXY_METHOD_WITH_RET(GDALRasterBlock *, nullptr, GetLockedBlockRef,
                         (int nXBlockOff, int nYBlockOff, int bJustInitialize),
                         (nXBlockOff, nYBlockOff, bJustInitialize))

RB_PROXY_METHOD_WITH_RET(GDALRasterBlock *, nullptr, TryGetLockedBlockRef,
                         (int nXBlockOff, int nYBlockOff),
                         (nXBlockOff, nYBlockOff))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, FlushBlock,
                         (int nXBlockOff, int nYBlockOff, int bWriteDirtyBlock),
                         (nXBlockOff, nYBlockOff, bWriteDirtyBlock))

CPLErr GDALProxyRasterBand::FlushCache(bool bAtClosing)
{
    // We need to make sure that all cached bocks at the proxy level are
    // first flushed
    CPLErr ret = GDALRasterBand::FlushCache(bAtClosing);
    if (ret == CE_None)
    {
        GDALRasterBand *poSrcBand = RefUnderlyingRasterBand();
        if (poSrcBand)
        {
            ret = poSrcBand->FlushCache(bAtClosing);
            UnrefUnderlyingRasterBand(poSrcBand);
        }
        else
        {
            ret = CE_Failure;
        }
    }
    return ret;
}

RB_PROXY_METHOD_WITH_RET(char **, nullptr, GetCategoryNames, (), ())
RB_PROXY_METHOD_WITH_RET(double, 0, GetNoDataValue, (int *pbSuccess),
                         (pbSuccess))
RB_PROXY_METHOD_WITH_RET(double, 0, GetMinimum, (int *pbSuccess), (pbSuccess))
RB_PROXY_METHOD_WITH_RET(double, 0, GetMaximum, (int *pbSuccess), (pbSuccess))
RB_PROXY_METHOD_WITH_RET(double, 0, GetOffset, (int *pbSuccess), (pbSuccess))
RB_PROXY_METHOD_WITH_RET(double, 0, GetScale, (int *pbSuccess), (pbSuccess))
RB_PROXY_METHOD_WITH_RET(const char *, nullptr, GetUnitType, (), ())
RB_PROXY_METHOD_WITH_RET(GDALColorInterp, GCI_Undefined, GetColorInterpretation,
                         (), ())
RB_PROXY_METHOD_WITH_RET(GDALColorTable *, nullptr, GetColorTable, (), ())
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, Fill,
                         (double dfRealValue, double dfImaginaryValue),
                         (dfRealValue, dfImaginaryValue))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetCategoryNames, (char **arg),
                         (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetNoDataValue, (double arg),
                         (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, DeleteNoDataValue, (), ())
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetColorTable,
                         (GDALColorTable * arg), (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetColorInterpretation,
                         (GDALColorInterp arg), (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetOffset, (double arg), (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetScale, (double arg), (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetUnitType, (const char *arg),
                         (arg))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, GetStatistics,
                         (int bApproxOK, int bForce, double *pdfMin,
                          double *pdfMax, double *pdfMean, double *padfStdDev),
                         (bApproxOK, bForce, pdfMin, pdfMax, pdfMean,
                          padfStdDev))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, ComputeStatistics,
                         (int bApproxOK, double *pdfMin, double *pdfMax,
                          double *pdfMean, double *pdfStdDev,
                          GDALProgressFunc pfn, void *pProgressData),
                         (bApproxOK, pdfMin, pdfMax, pdfMean, pdfStdDev, pfn,
                          pProgressData))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetStatistics,
                         (double dfMin, double dfMax, double dfMean,
                          double dfStdDev),
                         (dfMin, dfMax, dfMean, dfStdDev))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, ComputeRasterMinMax,
                         (int arg1, double *arg2), (arg1, arg2))

RB_PROXY_METHOD_WITH_RET(int, 0, HasArbitraryOverviews, (), ())
RB_PROXY_METHOD_WITH_RET(int, 0, GetOverviewCount, (), ())
RB_PROXY_METHOD_WITH_RET(GDALRasterBand *, nullptr, GetOverview, (int arg1),
                         (arg1))
RB_PROXY_METHOD_WITH_RET(GDALRasterBand *, nullptr, GetRasterSampleOverview,
                         (GUIntBig arg1), (arg1))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, BuildOverviews,
                         (const char *arg1, int arg2, const int *arg3,
                          GDALProgressFunc arg4, void *arg5,
                          CSLConstList papszOptions),
                         (arg1, arg2, arg3, arg4, arg5, papszOptions))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, AdviseRead,
                         (int nXOff, int nYOff, int nXSize, int nYSize,
                          int nBufXSize, int nBufYSize, GDALDataType eDT,
                          char **papszOptions),
                         (nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                          eDT, papszOptions))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, GetHistogram,
                         (double dfMin, double dfMax, int nBuckets,
                          GUIntBig *panHistogram, int bIncludeOutOfRange,
                          int bApproxOK, GDALProgressFunc pfn,
                          void *pProgressData),
                         (dfMin, dfMax, nBuckets, panHistogram,
                          bIncludeOutOfRange, bApproxOK, pfn, pProgressData))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, GetDefaultHistogram,
                         (double *pdfMin, double *pdfMax, int *pnBuckets,
                          GUIntBig **ppanHistogram, int bForce,
                          GDALProgressFunc pfn, void *pProgressData),
                         (pdfMin, pdfMax, pnBuckets, ppanHistogram, bForce, pfn,
                          pProgressData))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetDefaultHistogram,
                         (double dfMin, double dfMax, int nBuckets,
                          GUIntBig *panHistogram),
                         (dfMin, dfMax, nBuckets, panHistogram))

RB_PROXY_METHOD_WITH_RET(GDALRasterAttributeTable *, nullptr, GetDefaultRAT, (),
                         ())
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetDefaultRAT,
                         (const GDALRasterAttributeTable *arg1), (arg1))

RB_PROXY_METHOD_WITH_RET(GDALRasterBand *, nullptr, GetMaskBand, (), ())
RB_PROXY_METHOD_WITH_RET(int, 0, GetMaskFlags, (), ())
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, CreateMaskBand, (int nFlagsIn),
                         (nFlagsIn))
RB_PROXY_METHOD_WITH_RET(bool, false, IsMaskBand, () const, ())
RB_PROXY_METHOD_WITH_RET(GDALMaskValueRange, GMVR_UNKNOWN, GetMaskValueRange,
                         () const, ())

RB_PROXY_METHOD_WITH_RET(CPLVirtualMem *, nullptr, GetVirtualMemAuto,
                         (GDALRWFlag eRWFlag, int *pnPixelSpace,
                          GIntBig *pnLineSpace, char **papszOptions),
                         (eRWFlag, pnPixelSpace, pnLineSpace, papszOptions))

RB_PROXY_METHOD_WITH_RET(
    CPLErr, CE_Failure, InterpolateAtPoint,
    (double dfPixel, double dfLine, GDALRIOResampleAlg eInterpolation,
     double *pdfRealValue, double *pdfImagValue = nullptr) const,
    (dfPixel, dfLine, eInterpolation, pdfRealValue, pdfImagValue))

void GDALProxyRasterBand::EnablePixelTypeSignedByteWarning(bool b)
{
    GDALRasterBand *poSrcBand = RefUnderlyingRasterBand();
    if (poSrcBand)
    {
        poSrcBand->EnablePixelTypeSignedByteWarning(b);
        UnrefUnderlyingRasterBand(poSrcBand);
    }
}

/************************************************************************/
/*                 UnrefUnderlyingRasterBand()                        */
/************************************************************************/

void GDALProxyRasterBand::UnrefUnderlyingRasterBand(
    GDALRasterBand * /* poUnderlyingRasterBand */) const
{
}

/*! @endcond */
