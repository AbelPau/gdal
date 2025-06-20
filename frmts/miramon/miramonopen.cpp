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
    if (hMMR->eAccess == MMRAccess::MMR_Update &&
        (hMMR->bTreeDirty || (hMMR->poDictionary != nullptr &&
                              hMMR->poDictionary->bDictionaryTextDirty)))
        MMRFlush(hMMR);

    int nRet = 0;

    delete hMMR->poRoot;

    if (hMMR->poDictionary != nullptr)
        delete hMMR->poDictionary;

    CPLFree(hMMR->pszDictionary);

    for (int i = 0; i < hMMR->nBands; i++)
    {
        delete hMMR->papoBand[i];
    }
    delete hMMR->papoBand;

    if (hMMR->pProParameters != nullptr)
    {
        Eprj_ProParameters *psProParams =
            (Eprj_ProParameters *)hMMR->pProParameters;

        CPLFree(psProParams->proExeName);
        CPLFree(psProParams->proName);
        CPLFree(psProParams->proSpheroid.sphereName);

        CPLFree(psProParams);
    }

    if (hMMR->pDatum != nullptr)
    {
        CPLFree(((Eprj_Datum *)hMMR->pDatum)->datumname);
        CPLFree(((Eprj_Datum *)hMMR->pDatum)->gridname);
        CPLFree(hMMR->pDatum);
    }

    delete hMMR->fRel;
    hMMR->fRel = nullptr;

    // ·$·TODO alliberar correctament
    /*if (hMMR->pMapInfo != nullptr)
    {
        CPLFree(((Eprj_MapInfo *)hMMR->pMapInfo)->proName);
        CPLFree(((Eprj_MapInfo *)hMMR->pMapInfo)->units);
        CPLFree(hMMR->pMapInfo);
    }*/

    delete hMMR;
    return nRet;
}

/************************************************************************/
/*                           MMRGetBandInfo()                           */
/************************************************************************/

CPLErr MMRGetBandInfo(MMRHandle hMMR, int nBand, MMDataType *eMMRDataType,
                      MMBytesPerPixel *eMMBytesPerPixel, int *pnBlockXSize,
                      int *pnBlockYSize, int *pnCompressionType)

{
    if (nBand < 0 || nBand > hMMR->nBands)
    {
        CPLAssert(false);
        return CE_Failure;
    }

    MMRBand *poBand = hMMR->papoBand[nBand - 1];

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
/*                          MMRSetBandNoData()                          */
/*                                                                      */
/*      attempts to set a no-data value on the given band               */
/************************************************************************/

CPLErr MMRSetBandNoData(MMRHandle hMMR, int nBand, double dfValue)

{
    if (nBand < 0 || nBand - 1 > hMMR->nBands)
    {
        CPLAssert(false);
        return CE_Failure;
    }

    MMRBand *poBand = hMMR->papoBand[nBand - 1];

    if (!poBand)
        return CE_Failure;

    return poBand->SetNoDataValue(dfValue);
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
/*                         MMRSetRasterBlock()                          */
/************************************************************************/

CPLErr MMRSetRasterBlock(MMRHandle hMMR, int nBand, int nXBlock, int nYBlock,
                         void *pData)

{
    if (nBand < 1 || nBand > hMMR->nBands)
        return CE_Failure;

    return hMMR->papoBand[nBand - 1]->SetRasterBlock(nXBlock, nYBlock, pData);
}

/************************************************************************/
/*                         MMRSetBandName()                             */
/************************************************************************/

void MMRSetBandName(MMRHandle hMMR, int nBand, const char *pszName)
{
    if (nBand < 1 || nBand > hMMR->nBands)
        return;

    hMMR->papoBand[nBand - 1]->SetBandName(pszName);
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
/*                        MMRInvGeoTransform()                          */
/************************************************************************/

static bool MMRInvGeoTransform(const double *gt_in, double *gt_out)

{
    // Assume a 3rd row that is [1 0 0].
    // Compute determinate.
    const double det = gt_in[1] * gt_in[5] - gt_in[2] * gt_in[4];

    if (fabs(det) < 1.0e-15)
        return false;

    const double inv_det = 1.0 / det;

    // Compute adjoint, and divide by determinate.
    gt_out[1] = gt_in[5] * inv_det;
    gt_out[4] = -gt_in[4] * inv_det;

    gt_out[2] = -gt_in[2] * inv_det;
    gt_out[5] = gt_in[1] * inv_det;

    gt_out[0] = (gt_in[2] * gt_in[3] - gt_in[0] * gt_in[5]) * inv_det;
    gt_out[3] = (-gt_in[1] * gt_in[3] + gt_in[0] * gt_in[4]) * inv_det;

    return true;
}

/************************************************************************/
/*                           MMRSetMapInfo()                            */
/************************************************************************/

CPLErr MMRSetMapInfo(MMRHandle hMMR, const Eprj_MapInfo *poMapInfo)

{
    // Loop over bands, setting information on each one.
    for (int iBand = 0; iBand < hMMR->nBands; iBand++)
    {
        // Create a new Map_Info if there isn't one present already.
        MMREntry *poMIEntry =
            hMMR->papoBand[iBand]->poNode->GetNamedChild("Map_Info");
        if (poMIEntry == nullptr)
        {
            poMIEntry = MMREntry::New(hMMR, "Map_Info", "Eprj_MapInfo",
                                      hMMR->papoBand[iBand]->poNode);
        }

        poMIEntry->MarkDirty();

        // Ensure we have enough space for all the data.
        // TODO(schwehr): Explain 48 and 40 constants.
        const int nSize =
            static_cast<int>(48 + 40 + strlen(poMapInfo->proName) + 1 +
                             strlen(poMapInfo->units) + 1);

        GByte *pabyData = poMIEntry->MakeData(nSize);
        memset(pabyData, 0, nSize);

        poMIEntry->SetPosition();

        // Write the various fields.
        poMIEntry->SetStringField("proName", poMapInfo->proName);

        poMIEntry->SetDoubleField("upperLeftCenter.x",
                                  poMapInfo->upperLeftCenter.x);
        poMIEntry->SetDoubleField("upperLeftCenter.y",
                                  poMapInfo->upperLeftCenter.y);

        poMIEntry->SetDoubleField("lowerRightCenter.x",
                                  poMapInfo->lowerRightCenter.x);
        poMIEntry->SetDoubleField("lowerRightCenter.y",
                                  poMapInfo->lowerRightCenter.y);

        poMIEntry->SetDoubleField("pixelSize.width",
                                  poMapInfo->pixelSize.width);
        poMIEntry->SetDoubleField("pixelSize.height",
                                  poMapInfo->pixelSize.height);

        poMIEntry->SetStringField("units", poMapInfo->units);
    }

    return CE_None;
}

/************************************************************************/
/*                           MMRGetPEString()                           */
/*                                                                      */
/*      Some files have a ProjectionX node containing the ESRI style    */
/*      PE_STRING.  This function allows fetching from it.              */
/************************************************************************/

char *MMRGetPEString(MMRHandle hMMR)

{
    if (hMMR->nBands == 0)
        return nullptr;

    // ·$·TODO que es aixo?
    // Get the MMR node.
    MMREntry *poProX = hMMR->papoBand[0]->poNode->GetNamedChild("ProjectionX");
    if (poProX == nullptr)
        return nullptr;

    const char *pszType = poProX->GetStringField("projection.type.string");
    if (pszType == nullptr || !EQUAL(pszType, "PE_COORDSYS"))
        return nullptr;

    // Use a gross hack to scan ahead to the actual projection
    // string. We do it this way because we don't have general
    // handling for MIFObjects.
    GByte *pabyData = poProX->GetData();
    int nDataSize = poProX->GetDataSize();

    while (nDataSize > 10 &&
           !STARTS_WITH_CI((const char *)pabyData, "PE_COORDSYS,."))
    {
        pabyData++;
        nDataSize--;
    }

    if (nDataSize < 31)
        return nullptr;

    // Skip ahead to the actual string.
    pabyData += 30;
    // nDataSize -= 30;

    return CPLStrdup((const char *)pabyData);
}

/************************************************************************/
/*                            MMRGetDatum()                             */
/************************************************************************/

const Eprj_Datum *MMRGetDatum(MMRHandle hMMR)

{
    if (hMMR->nBands < 1)
        return nullptr;

    // Do we already have it?
    if (hMMR->pDatum != nullptr)
        return (Eprj_Datum *)hMMR->pDatum;

    // ·$·TODO que es aixo?
    // ·$·TODO Llegir el sistema de referencia
    // Get the MMR node.
    MMREntry *poMIEntry =
        hMMR->papoBand[0]->poNode->GetNamedChild("Projection.Datum");
    if (poMIEntry == nullptr)
        return nullptr;

    // Allocate the structure.
    Eprj_Datum *psDatum =
        static_cast<Eprj_Datum *>(CPLCalloc(sizeof(Eprj_Datum), 1));

    // Fetch the fields.
    psDatum->datumname = CPLStrdup(poMIEntry->GetStringField("datumname"));
    const int nDatumType = poMIEntry->GetIntField("type");
    if (nDatumType < 0 || nDatumType > EPRJ_DATUM_NONE)
    {
        CPLDebug("MiraMonRaster", "Invalid value for datum type: %d",
                 nDatumType);
        psDatum->type = EPRJ_DATUM_NONE;
    }
    else
        psDatum->type = static_cast<Eprj_DatumType>(nDatumType);

    for (int i = 0; i < 7; i++)
    {
        char pszBandSectionKey[30] = {};
        snprintf(pszBandSectionKey, sizeof(pszBandSectionKey), "params[%d]", i);
        psDatum->params[i] = poMIEntry->GetDoubleField(pszBandSectionKey);
    }

    psDatum->gridname = CPLStrdup(poMIEntry->GetStringField("gridname"));

    hMMR->pDatum = (void *)psDatum;

    return psDatum;
}

/************************************************************************/
/*                            MMRSetDatum()                             */
/************************************************************************/

CPLErr MMRSetDatum(MMRHandle hMMR, const Eprj_Datum *poDatum)

{
    // Loop over bands, setting information on each one.
    for (int iBand = 0; iBand < hMMR->nBands; iBand++)
    {
        // Create a new Projection if there isn't one present already.
        MMREntry *poProParams =
            hMMR->papoBand[iBand]->poNode->GetNamedChild("Projection");
        if (poProParams == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Can't add Eprj_Datum with no Eprj_ProjParameters.");
            return CE_Failure;
        }

        MMREntry *poDatumEntry = poProParams->GetNamedChild("Datum");
        if (poDatumEntry == nullptr)
        {
            poDatumEntry =
                MMREntry::New(hMMR, "Datum", "Eprj_Datum", poProParams);
        }

        poDatumEntry->MarkDirty();

        // Ensure we have enough space for all the data.
        // TODO(schwehr): Explain constants.
        int nSize =
            static_cast<int>(26 + strlen(poDatum->datumname) + 1 + 7 * 8);

        if (poDatum->gridname != nullptr)
            nSize += static_cast<int>(strlen(poDatum->gridname) + 1);

        GByte *pabyData = poDatumEntry->MakeData(nSize);
        if (!pabyData)
            return CE_Failure;

        poDatumEntry->SetPosition();

        // Initialize the whole thing to zeros for a clean start.
        memset(poDatumEntry->GetData(), 0, poDatumEntry->GetDataSize());

        // Write the various fields.
        poDatumEntry->SetStringField("datumname", poDatum->datumname);
        poDatumEntry->SetIntField("type", poDatum->type);

        poDatumEntry->SetDoubleField("params[0]", poDatum->params[0]);
        poDatumEntry->SetDoubleField("params[1]", poDatum->params[1]);
        poDatumEntry->SetDoubleField("params[2]", poDatum->params[2]);
        poDatumEntry->SetDoubleField("params[3]", poDatum->params[3]);
        poDatumEntry->SetDoubleField("params[4]", poDatum->params[4]);
        poDatumEntry->SetDoubleField("params[5]", poDatum->params[5]);
        poDatumEntry->SetDoubleField("params[6]", poDatum->params[6]);

        poDatumEntry->SetStringField("gridname", poDatum->gridname);
    }

    return CE_None;
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
/*                             MMRSetPCT()                              */
/*                                                                      */
/*      Set the PCT on a band.                                          */
/************************************************************************/

CPLErr MMRSetPCT(MMRHandle hMMR, int nBand, int nColors, double *padfRed,
                 double *padfGreen, double *padfBlue, double *padfAlpha)

{
    if (nBand < 1 || nBand > hMMR->nBands)
        return CE_Failure;

    return hMMR->papoBand[nBand - 1]->SetPCT(nColors, padfRed, padfGreen,
                                             padfBlue, padfAlpha);
}

/************************************************************************/
/*                          MMRGetDataRange()                           */
/************************************************************************/

CPLErr MMRGetDataRange(MMRHandle hMMR, int nBand, double *pdfMin,
                       double *pdfMax)

{
    if (nBand < 1 || nBand > hMMR->nBands)
        return CE_Failure;

    MMREntry *poBinInfo =
        hMMR->papoBand[nBand - 1]->poNode->GetNamedChild("Statistics");

    if (poBinInfo == nullptr)
        return CE_Failure;

    *pdfMin = poBinInfo->GetDoubleField("minimum");
    *pdfMax = poBinInfo->GetDoubleField("maximum");

    if (*pdfMax > *pdfMin)
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                         MMRDumpDictionary()                          */
/*                                                                      */
/*      Dump the dictionary (in raw, and parsed form) to the named      */
/*      device.                                                         */
/************************************************************************/

void MMRDumpDictionary(MMRHandle hMMR, FILE *fpOut)

{
    fprintf(fpOut, "%s\n", hMMR->pszDictionary);

    hMMR->poDictionary->Dump(fpOut);
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

/* ==================================================================== */
/*      Default data dictionary.  Emitted verbatim into the imagine     */
/*      file.                                                           */
/* ==================================================================== */

//·$·TODO que es aixo?
static const char *const aszDefaultDD[] = {
    "{1:lversion,1:LfreeList,1:LrootEntryPtr,1:sentryHeaderLength,1:"
    "LdictionaryPtr,}Emmr_File,{1:Lnext,1:Lprev,1:Lparent,1:Lchild,1:Ldata,1:"
    "ldataSize,64:cname,32:ctype,1:tmodTime,}Emmr_Entry,{16:clabel,1:"
    "LheaderPtr,}Emiramon_HeaderTag,{1:LfreeList,1:lfreeSize,}Ehfa_"
    "FreeListNode,{1:"
    "lsize,1:Lptr,}Ehfa_Data,{1:lwidth,1:lheight,1:e3:thematic,athematic,fft "
    "of real-valued data,layerType,",
    "1:e13:u1,u2,u4,u8,s8,u16,s16,u32,s32,f32,f64,c64,c128,pixelType,1:"
    "lblockWidth,1:lblockHeight,}Eimg_Layer,{1:lwidth,1:lheight,1:e3:thematic,"
    "athematic,fft of real-valued "
    "data,layerType,1:e13:u1,u2,u4,u8,s8,u16,s16,u32,s32,f32,f64,c64,c128,"
    "pixelType,1:lblockWidth,1:lblockHeight,}Eimg_Layer_SubSample,{1:e2:raster,"
    "vector,type,1:LdictionaryPtr,}Emmr_Layer,{1:LspaceUsedForRasterData,}"
    "ImgFormatInfo831,{1:sfileCode,1:Loffset,1:lsize,1:e2:false,true,logvalid,",
    "1:e2:no compression,ESRI GRID "
    "compression,compressionType,}Edms_VirtualBlockInfo,{1:lmin,1:lmax,}Edms_"
    "FreeIDList,{1:lnumvirtualblocks,1:lnumobjectsperblock,1:lnextobjectnum,1:"
    "e2:no compression,RLC "
    "compression,compressionType,0:poEdms_VirtualBlockInfo,blockinfo,0:poEdms_"
    "FreeIDList,freelist,1:tmodTime,}Edms_State,{0:pcstring,}Emif_String,{1:"
    "oEmif_String,fileName,2:LlayerStackValidFlagsOffset,2:"
    "LlayerStackDataOffset,1:LlayerStackCount,1:LlayerStackIndex,}"
    "ImgExternalRaster,{1:oEmif_String,algorithm,0:poEmif_String,nameList,}"
    "Eimg_RRDNamesList,{1:oEmif_String,projection,1:oEmif_String,units,}Eimg_"
    "MapInformation,",
    "{1:oEmif_String,dependent,}Eimg_DependentFile,{1:oEmif_String,"
    "ImageLayerName,}Eimg_DependentLayerName,{1:lnumrows,1:lnumcolumns,1:e13:"
    "EGDA_TYPE_U1,EGDA_TYPE_U2,EGDA_TYPE_U4,EGDA_TYPE_U8,EGDA_TYPE_S8,EGDA_"
    "TYPE_U16,EGDA_TYPE_S16,EGDA_TYPE_U32,EGDA_TYPE_S32,EGDA_TYPE_F32,EGDA_"
    "TYPE_F64,EGDA_TYPE_C64,EGDA_TYPE_C128,datatype,1:e4:EGDA_SCALAR_OBJECT,"
    "EGDA_TABLE_OBJECT,EGDA_MATRIX_OBJECT,EGDA_RASTER_OBJECT,objecttype,}Egda_"
    "BaseData,{1:*bvalueBD,}Eimg_NonInitializedValue,{1:dx,1:dy,}Eprj_"
    "Coordinate,{1:dwidth,1:dheight,}Eprj_Size,{0:pcproName,1:*oEprj_"
    "Coordinate,upperLeftCenter,",
    "1:*oEprj_Coordinate,lowerRightCenter,1:*oEprj_Size,pixelSize,0:pcunits,}"
    "Eprj_MapInfo,{0:pcdatumname,1:e3:EPRJ_DATUM_PARAMETRIC,EPRJ_DATUM_GRID,"
    "EPRJ_DATUM_REGRESSION,type,0:pdparams,0:pcgridname,}Eprj_Datum,{0:"
    "pcsphereName,1:da,1:db,1:deSquared,1:dradius,}Eprj_Spheroid,{1:e2:EPRJ_"
    "INTERNAL,EPRJ_EXTERNAL,proType,1:lproNumber,0:pcproExeName,0:pcproName,1:"
    "lproZone,0:pdproParams,1:*oEprj_Spheroid,proSpheroid,}Eprj_ProParameters,{"
    "1:dminimum,1:dmaximum,1:dmean,1:dmedian,1:dmode,1:dstddev,}Esta_"
    "Statistics,{1:lnumBins,1:e4:direct,linear,logarithmic,explicit,"
    "binFunctionType,1:dminLimit,1:dmaxLimit,1:*bbinLimits,}Edsc_BinFunction,{"
    "0:poEmif_String,LayerNames,1:*bExcludedValues,1:oEmif_String,AOIname,",
    "1:lSkipFactorX,1:lSkipFactorY,1:*oEdsc_BinFunction,BinFunction,}Eimg_"
    "StatisticsParameters830,{1:lnumrows,}Edsc_Table,{1:lnumRows,1:"
    "LcolumnDataPtr,1:e4:integer,real,complex,string,dataType,1:lmaxNumChars,}"
    "Edsc_Column,{1:lposition,0:pcname,1:e2:EMSC_FALSE,EMSC_TRUE,editable,1:e3:"
    "LEFT,CENTER,RIGHT,alignment,0:pcformat,1:e3:DEFAULT,APPLY,AUTO-APPLY,"
    "formulamode,0:pcformula,1:dcolumnwidth,0:pcunits,1:e5:NO_COLOR,RED,GREEN,"
    "BLUE,COLOR,colorflag,0:pcgreenname,0:pcbluename,}Eded_ColumnAttributes_1,{"
    "1:lversion,1:lnumobjects,1:e2:EAOI_UNION,EAOI_INTERSECTION,operation,}"
    "Eaoi_AreaOfInterest,",
    "{1:x{0:pcstring,}Emif_String,type,1:x{0:pcstring,}Emif_String,"
    "MIFDictionary,0:pCMIFObject,}Emif_MIFObject,",
    "{1:x{1:x{0:pcstring,}Emif_String,type,1:x{0:pcstring,}Emif_String,"
    "MIFDictionary,0:pCMIFObject,}Emif_MIFObject,projection,1:x{0:pcstring,}"
    "Emif_String,title,}Eprj_MapProjection842,",
    "{0:poEmif_String,titleList,}Exfr_GenericXFormHeader,{1:lorder,1:"
    "lnumdimtransform,1:lnumdimpolynomial,1:ltermcount,0:plexponentlist,1:*"
    "bpolycoefmtx,1:*bpolycoefvector,}Efga_Polynomial,",
    ".",
    nullptr};

/************************************************************************/
/*                            MMRCreateLL()                             */
/*                                                                      */
/*      Low level creation of an Imagine file.  Writes out the          */
/*      Emiramon_HeaderTag, dictionary and Emmr_File.                       */
/************************************************************************/

MMRHandle MMRCreateLL(const char *pszFileName)

{
    // Create the file in the file system.
    VSILFILE *fp = VSIFOpenL(pszFileName, "w+b");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Creation of file %s failed.",
                 pszFileName);
        return nullptr;
    }

    // Create the MMRInfo_t.
    MMRInfo_t *psInfo =
        static_cast<MMRInfo_t *>(CPLCalloc(sizeof(MMRInfo_t), 1));

    psInfo->fp = fp;
    psInfo->eAccess = MMRAccess::MMR_Update;
    psInfo->nXSize = 0;
    psInfo->nYSize = 0;
    psInfo->nBands = 0;
    psInfo->papoBand = nullptr;
    psInfo->pMapInfo = nullptr;
    psInfo->pDatum = nullptr;
    psInfo->pProParameters = nullptr;
    psInfo->bTreeDirty = false;

    //·$·TODO això no sha descriure
    // Write out the Emiramon_HeaderTag.
    bool bRet = VSIFWriteL((void *)"EMMR_HEADER_TAG", 1, 16, fp) > 0;

    GInt32 nHeaderPos = 20;
    MMRStandard(4, &nHeaderPos);
    bRet &= VSIFWriteL(&nHeaderPos, 4, 1, fp) > 0;

    // Write the Emmr_File node, locked in at offset 20.
    GInt32 nVersion = 1;
    GInt32 nFreeList = 0;
    GInt32 nRootEntry = 0;
    GInt16 nEntryHeaderLength = 128;
    GInt32 nDictionaryPtr = 38;

    psInfo->nEntryHeaderLength = nEntryHeaderLength;
    psInfo->nRootPos = 0;
    psInfo->nDictionaryPos = nDictionaryPtr;
    psInfo->nVersion = nVersion;

    MMRStandard(4, &nVersion);
    MMRStandard(4, &nFreeList);
    MMRStandard(4, &nRootEntry);
    MMRStandard(2, &nEntryHeaderLength);
    MMRStandard(4, &nDictionaryPtr);

    bRet &= VSIFWriteL(&nVersion, 4, 1, fp) > 0;
    bRet &= VSIFWriteL(&nFreeList, 4, 1, fp) > 0;
    bRet &= VSIFWriteL(&nRootEntry, 4, 1, fp) > 0;
    bRet &= VSIFWriteL(&nEntryHeaderLength, 2, 1, fp) > 0;
    bRet &= VSIFWriteL(&nDictionaryPtr, 4, 1, fp) > 0;

    // Write the dictionary, locked in at location 38.  Note that
    // we jump through a bunch of hoops to operate on the
    // dictionary in chunks because some compiles (such as VC++)
    // don't allow particularly large static strings.
    int nDictLen = 0;

    for (int iChunk = 0; aszDefaultDD[iChunk] != nullptr; iChunk++)
        nDictLen += static_cast<int>(strlen(aszDefaultDD[iChunk]));

    psInfo->pszDictionary = static_cast<char *>(CPLMalloc(nDictLen + 1));
    psInfo->pszDictionary[0] = '\0';

    for (int iChunk = 0; aszDefaultDD[iChunk] != nullptr; iChunk++)
        strcat(psInfo->pszDictionary, aszDefaultDD[iChunk]);

    bRet &= VSIFWriteL((void *)psInfo->pszDictionary,
                       strlen(psInfo->pszDictionary) + 1, 1, fp) > 0;
    if (!bRet)
    {
        CPL_IGNORE_RET_VAL(MMRClose(psInfo));
        return nullptr;
    }

    psInfo->poDictionary = new MMRDictionary(psInfo->pszDictionary);

    psInfo->nEndOfFile = static_cast<GUInt32>(VSIFTellL(fp));

    // Create a root entry.
    psInfo->poRoot = new MMREntry(psInfo, "root", "root", nullptr);

    // If an .ige or .rrd file exists with the same base name,
    // delete them.  (#1784)
    CPLString osExtension = CPLGetExtensionSafe(pszFileName);
    if (!EQUAL(osExtension, "rrd") && !EQUAL(osExtension, "aux"))
    {
        CPLString osPath = CPLGetPathSafe(pszFileName);
        CPLString osBasename = CPLGetBasenameSafe(pszFileName);
        VSIStatBufL sStatBuf;
        CPLString osSupFile = CPLFormCIFilenameSafe(osPath, osBasename, "rrd");

        if (VSIStatL(osSupFile, &sStatBuf) == 0)
            VSIUnlink(osSupFile);

        osSupFile = CPLFormCIFilenameSafe(osPath, osBasename, "ige");

        if (VSIStatL(osSupFile, &sStatBuf) == 0)
            VSIUnlink(osSupFile);
    }

    return psInfo;
}

/************************************************************************/
/*                          MMRAllocateSpace()                          */
/*                                                                      */
/*      Return an area in the file to the caller to write the           */
/*      requested number of bytes.  Currently this is always at the     */
/*      end of the file, but eventually we might actually keep track    */
/*      of free space.  The MMRInfo_t's concept of file size is         */
/*      updated, even if nothing ever gets written to this region.      */
/*                                                                      */
/*      Returns the offset to the requested space, or zero one          */
/*      failure.                                                        */
/************************************************************************/

GUInt32 MMRAllocateSpace(MMRInfo_t *psInfo, GUInt32 nBytes)

{
    // TODO(schwehr): Check if this will wrap over 2GB limit.

    psInfo->nEndOfFile += nBytes;
    return psInfo->nEndOfFile - nBytes;
}

/************************************************************************/
/*                              MMRFlush()                              */
/*                                                                      */
/*      Write out any dirty tree information to disk, putting the       */
/*      disk file in a consistent state.                                */
/************************************************************************/

CPLErr MMRFlush(MMRHandle hMMR)

{
    if (!hMMR->bTreeDirty && !hMMR->poDictionary->bDictionaryTextDirty)
        return CE_None;

    CPLAssert(hMMR->poRoot != nullptr);

    // Flush MMREntry tree to disk.
    if (hMMR->bTreeDirty)
    {
        const CPLErr eErr = hMMR->poRoot->FlushToDisk();
        if (eErr != CE_None)
            return eErr;

        hMMR->bTreeDirty = false;
    }

    // Flush Dictionary to disk.
    GUInt32 nNewDictionaryPos = hMMR->nDictionaryPos;
    bool bRet = true;
    if (hMMR->poDictionary->bDictionaryTextDirty)
    {
        bRet &= VSIFSeekL(hMMR->fp, 0, SEEK_END) >= 0;
        nNewDictionaryPos = static_cast<GUInt32>(VSIFTellL(hMMR->fp));
        bRet &=
            VSIFWriteL(hMMR->poDictionary->osDictionaryText.c_str(),
                       strlen(hMMR->poDictionary->osDictionaryText.c_str()) + 1,
                       1, hMMR->fp) > 0;
        hMMR->poDictionary->bDictionaryTextDirty = false;
    }

    // Do we need to update the Emmr_File pointer to the root node?
    if (hMMR->nRootPos != hMMR->poRoot->GetFilePos() ||
        nNewDictionaryPos != hMMR->nDictionaryPos)
    {
        GUInt32 nHeaderPos = 0;

        bRet &= VSIFSeekL(hMMR->fp, 16, SEEK_SET) >= 0;
        bRet &= VSIFReadL(&nHeaderPos, sizeof(GInt32), 1, hMMR->fp) > 0;
        MMRStandard(4, &nHeaderPos);

        GUInt32 nOffset = hMMR->poRoot->GetFilePos();
        hMMR->nRootPos = nOffset;
        MMRStandard(4, &nOffset);
        bRet &= VSIFSeekL(hMMR->fp, nHeaderPos + 8, SEEK_SET) >= 0;
        bRet &= VSIFWriteL(&nOffset, 4, 1, hMMR->fp) > 0;

        nOffset = nNewDictionaryPos;
        hMMR->nDictionaryPos = nNewDictionaryPos;
        MMRStandard(4, &nOffset);
        bRet &= VSIFSeekL(hMMR->fp, nHeaderPos + 14, SEEK_SET) >= 0;
        bRet &= VSIFWriteL(&nOffset, 4, 1, hMMR->fp) > 0;
    }

    return bRet ? CE_None : CE_Failure;
}

/************************************************************************/
/*                           MMRCreateLayer()                           */
/*                                                                      */
/*      Create a layer object, and corresponding RasterDMS.             */
/*      Suitable for use with primary layers, and overviews.            */
/************************************************************************/

int MMRCreateLayer(MMRHandle psInfo, MMREntry *poParent,
                   const char *pszLayerName, int bOverview, int nBlockSize,
                   int bCreateCompressed, int bCreateLargeRaster,
                   int bDependentLayer, int nXSize, int nYSize,
                   EPTType eDataType, char ** /* papszOptions */,
                   // These are only related to external (large) files.
                   //GIntBig nStackValidFlagsOffset, GIntBig nStackDataOffset,
                   int nStackCount, int nStackIndex)

{
    const char *pszLayerType =
        bOverview ? "Eimg_Layer_SubSample" : "Eimg_Layer";

    if (nBlockSize <= 0)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "MMRCreateLayer: nBlockXSize < 0");
        return FALSE;
    }

    // Work out some details about the tiling scheme.
    const int nBlocksPerRow =
        DIV_ROUND_UP((nXSize + nBlockSize - 1), nBlockSize)
            const int nBlocksPerColumn =
                DIV_ROUND_UP((nYSize + nBlockSize - 1), nBlockSize);
    const int nBlocks = nBlocksPerRow * nBlocksPerColumn;
    const int nBytesPerBlock =
        (nBlockSize * nBlockSize * MMRGetDataTypeBits(eDataType) + 7) / 8;

    // Create the Eimg_Layer for the band.
    MMREntry *poEimg_Layer =
        MMREntry::New(psInfo, pszLayerName, pszLayerType, poParent);

    poEimg_Layer->SetIntField("width", nXSize);
    poEimg_Layer->SetIntField("height", nYSize);
    poEimg_Layer->SetStringField("layerType", "athematic");
    poEimg_Layer->SetIntField("pixelType", eDataType);
    poEimg_Layer->SetIntField("blockWidth", nBlockSize);
    poEimg_Layer->SetIntField("blockHeight", nBlockSize);

    //·$·TODO el quit de la questio?
    // Create the RasterDMS (block list).  This is a complex type
    // with pointers, and variable size.  We set the superstructure
    // ourselves rather than trying to have the MMR type management
    // system do it for us (since this would be hard to implement).
    if (!bCreateLargeRaster && !bDependentLayer)
    {
        MMREntry *poEdms_State =
            MMREntry::New(psInfo, "RasterDMS", "Edms_State", poEimg_Layer);

        // TODO(schwehr): Explain constants.
        const int nDmsSize = 14 * nBlocks + 38;
        GByte *pabyData = poEdms_State->MakeData(nDmsSize);

        // Set some simple values.
        poEdms_State->SetIntField("numvirtualblocks", nBlocks);
        poEdms_State->SetIntField("numobjectsperblock",
                                  nBlockSize * nBlockSize);
        poEdms_State->SetIntField("nextobjectnum",
                                  nBlockSize * nBlockSize * nBlocks);

        // Is file compressed or not?
        if (bCreateCompressed)
        {
            poEdms_State->SetStringField("compressionType", "RLC compression");
        }
        else
        {
            poEdms_State->SetStringField("compressionType", "no compression");
        }

        // We need to hardcode file offset into the data, so locate it now.
        poEdms_State->SetPosition();

        // Set block info headers.

        // Blockinfo count.
        GUInt32 nValue = nBlocks;
        MMRStandard(4, &nValue);
        memcpy(pabyData + 14, &nValue, 4);

        // Blockinfo position.
        nValue = poEdms_State->GetDataPos() + 22;
        MMRStandard(4, &nValue);
        memcpy(pabyData + 18, &nValue, 4);

        // Set each blockinfo.
        for (int iBlock = 0; iBlock < nBlocks; iBlock++)
        {
            int nOffset = 22 + 14 * iBlock;

            // fileCode.
            GInt16 nValue16 = 0;
            MMRStandard(2, &nValue16);
            memcpy(pabyData + nOffset, &nValue16, 2);

            // Offset.
            if (bCreateCompressed)
            {
                // Flag it with zero offset. Allocate space when we compress it.
                nValue = 0;
            }
            else
            {
                nValue = MMRAllocateSpace(psInfo, nBytesPerBlock);
            }
            MMRStandard(4, &nValue);
            memcpy(pabyData + nOffset + 2, &nValue, 4);

            // Size.
            if (bCreateCompressed)
            {
                // Flag with zero size. Don't know until we compress it.
                nValue = 0;
            }
            else
            {
                nValue = nBytesPerBlock;
            }
            MMRStandard(4, &nValue);
            memcpy(pabyData + nOffset + 6, &nValue, 4);

            // logValid (false).
            nValue16 = 0;
            MMRStandard(2, &nValue16);
            memcpy(pabyData + nOffset + 10, &nValue16, 2);

            // compressionType.
            if (bCreateCompressed)
                nValue16 = 1;
            else
                nValue16 = 0;

            MMRStandard(2, &nValue16);
            memcpy(pabyData + nOffset + 12, &nValue16, 2);
        }
    }

    // Create ExternalRasterDMS object.
    else if (bCreateLargeRaster)
    {
        MMREntry *poEdms_State = MMREntry::New(
            psInfo, "ExternalRasterDMS", "ImgExternalRaster", poEimg_Layer);
        //poEdms_State->MakeData(
        //    static_cast<int>(8 + strlen(psInfo->pszIGEFilename) + 1 + 6 * 4));

        //poEdms_State->SetStringField("fileName.string", psInfo->pszIGEFilename);

        //poEdms_State->SetIntField(
        //    "layerStackValidFlagsOffset[0]",
        //    static_cast<int>(nStackValidFlagsOffset & 0xFFFFFFFF));
        //poEdms_State->SetIntField(
        //    "layerStackValidFlagsOffset[1]",
        //    static_cast<int>(nStackValidFlagsOffset >> 32));

        //poEdms_State->SetIntField(
        //    "layerStackDataOffset[0]",
        //    static_cast<int>(nStackDataOffset & 0xFFFFFFFF));
        //poEdms_State->SetIntField("layerStackDataOffset[1]",
        //                          static_cast<int>(nStackDataOffset >> 32));
        poEdms_State->SetIntField("layerStackCount", nStackCount);
        poEdms_State->SetIntField("layerStackIndex", nStackIndex);
    }
    // Dependent...
    else if (bDependentLayer)
    {
        MMREntry *poDepLayerName =
            MMREntry::New(psInfo, "DependentLayerName",
                          "Eimg_DependentLayerName", poEimg_Layer);
        poDepLayerName->MakeData(
            static_cast<int>(8 + strlen(pszLayerName) + 2));

        poDepLayerName->SetStringField("ImageLayerName.string", pszLayerName);
    }

    // Create the Emmr_Layer.
    char chBandType = '\0';

    if (eDataType == EPT_u1)
        chBandType = '1';
    else if (eDataType == EPT_u2)
        chBandType = '2';
    else if (eDataType == EPT_u4)
        chBandType = '4';
    else if (eDataType == EPT_u8)
        chBandType = 'c';
    else if (eDataType == EPT_s8)
        chBandType = 'C';
    else if (eDataType == EPT_u16)
        chBandType = 's';
    else if (eDataType == EPT_s16)
        chBandType = 'S';
    else if (eDataType == EPT_u32)
        // For some reason erdas imagine expects an L for unsigned 32 bit ints
        // otherwise it gives strange "out of memory errors".
        chBandType = 'L';
    else if (eDataType == EPT_s32)
        chBandType = 'L';
    else if (eDataType == EPT_f32)
        chBandType = 'f';
    else if (eDataType == EPT_f64)
        chBandType = 'd';
    else if (eDataType == EPT_c64)
        chBandType = 'm';
    else if (eDataType == EPT_c128)
        chBandType = 'M';
    else
    {
        CPLAssert(false);
        chBandType = 'c';
    }

    // The first value in the entry below gives the number of pixels
    // within a block.
    char szLDict[128] = {};
    snprintf(szLDict, sizeof(szLDict), "{%d:%cdata,}RasterDMS,.",
             nBlockSize * nBlockSize, chBandType);

    MMREntry *poEmmr_Layer =
        MMREntry::New(psInfo, "Emmr_Layer", "Emmr_Layer", poEimg_Layer);
    poEmmr_Layer->MakeData();
    poEmmr_Layer->SetPosition();
    const GUInt32 nLDict =
        MMRAllocateSpace(psInfo, static_cast<GUInt32>(strlen(szLDict) + 1));

    poEmmr_Layer->SetStringField("type", "raster");
    poEmmr_Layer->SetIntField("dictionaryPtr", nLDict);

    bool bRet = VSIFSeekL(psInfo->fp, nLDict, SEEK_SET) >= 0;
    bRet &= VSIFWriteL((void *)szLDict, strlen(szLDict) + 1, 1, psInfo->fp) > 0;

    return bRet;
}

/************************************************************************/
/*                             MMRCreate()                              */
/************************************************************************/

MMRHandle MMRCreate(const char *pszFileName, int nXSize, int nYSize, int nBands,
                    EPTType eDataType, char **papszOptions)

{
    int nBlockSize = 64;

    bool bCreateLargeRaster = CPLFetchBool(papszOptions, "USE_SPILL", false);
    bool bCreateCompressed = CPLFetchBool(papszOptions, "COMPRESS", false) ||
                             CPLFetchBool(papszOptions, "COMPRESSED", false);
    const bool bCreateAux = CPLFetchBool(papszOptions, "AUX", false);

    //char *pszFullFilename = nullptr;
    //char *pszRawFilename = nullptr;

    // Work out some details about the tiling scheme.
    const int nBlocksPerRow = DIV_ROUND_UP(nXSize, nBlockSize);
    const int nBlocksPerColumn = DIV_ROUND_UP(nYSize, nBlockSize);
    if (nBlocksPerRow > INT_MAX / nBlocksPerColumn)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too many blocks");
        return nullptr;
    }
    const int nBlocks = nBlocksPerRow * nBlocksPerColumn;
    const GInt64 nBytesPerBlock64 =
        (static_cast<GInt64>(nBlockSize) * nBlockSize *
             MMRGetDataTypeBits(eDataType) +
         7) /
        8;
    if (nBytesPerBlock64 > INT_MAX)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too large block");
        return nullptr;
    }
    const int nBytesPerBlock = static_cast<int>(nBytesPerBlock64);

    // Create the low level structure.
    MMRHandle psInfo = MMRCreateLL(pszFileName);
    if (psInfo == nullptr)
        return nullptr;

    CPLDebug("MMRCreate",
             "Blocks per row %d, blocks per column %d, "
             "total number of blocks %d, bytes per block %d.",
             nBlocksPerRow, nBlocksPerColumn, nBlocks, nBytesPerBlock);

    // Check whether we should create external large file with
    // image.  We create a spill file if the amount of imagery is
    // close to 2GB.  We don't check the amount of auxiliary
    // information, so in theory if there were an awful lot of
    // non-imagery data our approximate size could be smaller than
    // the file will actually we be.  We leave room for 10MB of
    // auxiliary data.
    // We can also force spill file creation using option
    // SPILL_FILE=YES.
    const double dfApproxSize = static_cast<double>(nBytesPerBlock) *
                                    static_cast<double>(nBlocks) *
                                    static_cast<double>(nBands) +
                                10000000.0;

    if (dfApproxSize > 2147483648.0 && !bCreateAux)
        bCreateLargeRaster = true;

    // creates this entry even if an external spill file is used.
    if (!bCreateAux)
    {
        MMREntry *poImgFormat = MMREntry::New(
            psInfo, "IMGFormatInfo", "ImgFormatInfo831", psInfo->poRoot);
        poImgFormat->MakeData();
        if (bCreateLargeRaster)
        {
            poImgFormat->SetIntField("spaceUsedForRasterData", 0);
            // Can't be compressed if we are creating a spillfile.
            bCreateCompressed = false;
        }
        else
        {
            poImgFormat->SetIntField("spaceUsedForRasterData",
                                     nBytesPerBlock * nBlocks * nBands);
        }
    }

    // Create external file and write its header.
    /*GIntBig nValidFlagsOffset = 0;
    GIntBig nDataOffset = 0;

    if (bCreateLargeRaster)
    {
        if (!MMRCreateSpillStack(psInfo, nXSize, nYSize, nBands, nBlockSize,
                                 eDataType, &nValidFlagsOffset, &nDataOffset))
        {
            CPLFree(pszRawFilename);
            CPLFree(pszFullFilename);
            return nullptr;
        }
    }*/

    // Create each band (layer).
    for (int iBand = 0; iBand < nBands; iBand++)
    {
        char szName[128] = {};

        snprintf(szName, sizeof(szName), "Layer_%d", iBand + 1);

        if (!MMRCreateLayer(psInfo, psInfo->poRoot, szName, FALSE, nBlockSize,
                            bCreateCompressed, bCreateLargeRaster, bCreateAux,
                            nXSize, nYSize, eDataType, papszOptions,
                            /*nValidFlagsOffset, nDataOffset, */ nBands, iBand))
        {
            CPL_IGNORE_RET_VAL(MMRClose(psInfo));
            return nullptr;
        }
    }

    // Initialize the band information.
    MMRRel::ParseBandInfo(psInfo);

    return psInfo;
}

/************************************************************************/
/*                           MMRGetMetadata()                           */
/*                                                                      */
/*      Read metadata structured in a table called GDAL_MetaData.       */
/************************************************************************/

char **MMRGetMetadata(MMRHandle hMMR, int nBand)

{
    MMREntry *poTable = nullptr;

    if (nBand > 0 && nBand <= hMMR->nBands)
        poTable = hMMR->papoBand[nBand - 1]->poNode->GetChild();
    else if (nBand == 0)
        poTable = hMMR->poRoot->GetChild();
    else
        return nullptr;

    for (; poTable != nullptr && !EQUAL(poTable->GetName(), "GDAL_MetaData");
         poTable = poTable->GetNext())
    {
    }

    if (poTable == nullptr || !EQUAL(poTable->GetType(), "Edsc_Table"))
        return nullptr;

    if (poTable->GetIntField("numRows") != 1)
    {
        CPLDebug("MMRDataset", "GDAL_MetaData.numRows = %d, expected 1!",
                 poTable->GetIntField("numRows"));
        return nullptr;
    }

    // Loop over each column.  Each column will be one metadata
    // entry, with the title being the key, and the row value being
    // the value.  There is only ever one row in GDAL_MetaData tables.
    char **papszMD = nullptr;

    for (MMREntry *poColumn = poTable->GetChild(); poColumn != nullptr;
         poColumn = poColumn->GetNext())
    {
        // Skip the #Bin_Function# entry.
        if (STARTS_WITH_CI(poColumn->GetName(), "#"))
            continue;

        const char *pszValue = poColumn->GetStringField("dataType");
        if (pszValue == nullptr || !EQUAL(pszValue, "string"))
            continue;

        const int columnDataPtr = poColumn->GetIntField("columnDataPtr");
        if (columnDataPtr <= 0)
            continue;

        // Read up to nMaxNumChars bytes from the indicated location.
        // allocate required space temporarily nMaxNumChars should have been
        // set by GDAL originally so we should trust it, but who knows.
        const int nMaxNumChars = poColumn->GetIntField("maxNumChars");

        if (nMaxNumChars <= 0)
        {
            papszMD = CSLSetNameValue(papszMD, poColumn->GetName(), "");
        }
        else
        {
            char *pszMDValue =
                static_cast<char *>(VSI_MALLOC_VERBOSE(nMaxNumChars));
            if (pszMDValue == nullptr)
            {
                continue;
            }

            if (VSIFSeekL(hMMR->fp, columnDataPtr, SEEK_SET) != 0)
            {
                CPLFree(pszMDValue);
                continue;
            }

            const int nMDBytes = static_cast<int>(
                VSIFReadL(pszMDValue, 1, nMaxNumChars, hMMR->fp));
            if (nMDBytes == 0)
            {
                CPLFree(pszMDValue);
                continue;
            }

            pszMDValue[nMaxNumChars - 1] = '\0';

            papszMD = CSLSetNameValue(papszMD, poColumn->GetName(), pszMDValue);
            CPLFree(pszMDValue);
        }
    }

    return papszMD;
}

/************************************************************************/
/*                         MMRSetGDALMetadata()                         */
/*                                                                      */
/*      This function is used to set metadata in a table called         */
/*      GDAL_MetaData.  It is called by MMRSetMetadata() for all        */
/*      metadata items that aren't some specific supported              */
/*      information (like histogram or stats info).                     */
/************************************************************************/

static CPLErr MMRSetGDALMetadata(MMRHandle hMMR, int nBand, char **papszMD)

{
    if (papszMD == nullptr)
        return CE_None;

    MMREntry *poNode = nullptr;

    if (nBand > 0 && nBand <= hMMR->nBands)
        poNode = hMMR->papoBand[nBand - 1]->poNode;
    else if (nBand == 0)
        poNode = hMMR->poRoot;
    else
        return CE_Failure;

    // Create the Descriptor table.
    // Check we have no table with this name already.
    MMREntry *poEdsc_Table = poNode->GetNamedChild("GDAL_MetaData");

    if (poEdsc_Table == nullptr ||
        !EQUAL(poEdsc_Table->GetType(), "Edsc_Table"))
        poEdsc_Table =
            MMREntry::New(hMMR, "GDAL_MetaData", "Edsc_Table", poNode);

    poEdsc_Table->SetIntField("numrows", 1);

    // Create the Binning function node.  Do we really need this though?
    // Check it doesn't exist already.
    MMREntry *poEdsc_BinFunction =
        poEdsc_Table->GetNamedChild("#Bin_Function#");

    if (poEdsc_BinFunction == nullptr ||
        !EQUAL(poEdsc_BinFunction->GetType(), "Edsc_BinFunction"))
        poEdsc_BinFunction = MMREntry::New(hMMR, "#Bin_Function#",
                                           "Edsc_BinFunction", poEdsc_Table);

    // Because of the BaseData we have to hardcode the size.
    poEdsc_BinFunction->MakeData(30);

    poEdsc_BinFunction->SetIntField("numBins", 1);
    poEdsc_BinFunction->SetStringField("binFunction", "direct");
    poEdsc_BinFunction->SetDoubleField("minLimit", 0.0);
    poEdsc_BinFunction->SetDoubleField("maxLimit", 0.0);

    // Process each metadata item as a separate column.
    bool bRet = true;
    for (int iColumn = 0; papszMD[iColumn] != nullptr; iColumn++)
    {
        char *pszKey = nullptr;
        const char *pszValue = CPLParseNameValue(papszMD[iColumn], &pszKey);
        if (pszValue == nullptr)
            continue;

        // Create the Edsc_Column.
        // Check it doesn't exist already.
        MMREntry *poEdsc_Column = poEdsc_Table->GetNamedChild(pszKey);

        if (poEdsc_Column == nullptr ||
            !EQUAL(poEdsc_Column->GetType(), "Edsc_Column"))
            poEdsc_Column =
                MMREntry::New(hMMR, pszKey, "Edsc_Column", poEdsc_Table);

        poEdsc_Column->SetIntField("numRows", 1);
        poEdsc_Column->SetStringField("dataType", "string");
        poEdsc_Column->SetIntField("maxNumChars",
                                   static_cast<GUInt32>(strlen(pszValue) + 1));

        // Write the data out.
        const int nOffset =
            MMRAllocateSpace(hMMR, static_cast<GUInt32>(strlen(pszValue) + 1));

        poEdsc_Column->SetIntField("columnDataPtr", nOffset);

        bRet &= VSIFSeekL(hMMR->fp, nOffset, SEEK_SET) >= 0;
        bRet &=
            VSIFWriteL((void *)pszValue, strlen(pszValue) + 1, 1, hMMR->fp) > 0;

        CPLFree(pszKey);
    }

    return bRet ? CE_None : CE_Failure;
}

/************************************************************************/
/*                           MMRSetMetadata()                           */
/************************************************************************/

CPLErr MMRSetMetadata(MMRHandle hMMR, int nBand, char **papszMD)

{
    char **papszGDALMD = nullptr;

    if (CSLCount(papszMD) == 0)
        return CE_None;

    MMREntry *poNode = nullptr;

    if (nBand > 0 && nBand <= hMMR->nBands)
        poNode = hMMR->papoBand[nBand - 1]->poNode;
    else if (nBand == 0)
        poNode = hMMR->poRoot;
    else
        return CE_Failure;
#ifdef DEBUG
    // To please Clang Static Analyzer (CSA).
    if (poNode == nullptr)
    {
        CPLAssert(false);
        return CE_Failure;
    }
#endif

    // Check if the Metadata is an "known" entity which should be
    // stored in a better place.
    char *pszBinValues = nullptr;
    bool bCreatedHistogramParameters = false;
    bool bCreatedStatistics = false;
    const char *const *pszAuxMetaData = GetMMRAuxMetaDataList();
    // Check each metadata item.
    for (int iColumn = 0; papszMD[iColumn] != nullptr; iColumn++)
    {
        char *pszKey = nullptr;
        const char *pszValue = CPLParseNameValue(papszMD[iColumn], &pszKey);
        if (pszValue == nullptr)
            continue;

        // Know look if its known.
        int i = 0;  // Used after for.
        for (; pszAuxMetaData[i] != nullptr; i += 4)
        {
            if (EQUALN(pszAuxMetaData[i + 2], pszKey, strlen(pszKey)))
                break;
        }
        if (pszAuxMetaData[i] != nullptr)
        {
            // Found one, get the right entry.
            MMREntry *poEntry = nullptr;

            if (strlen(pszAuxMetaData[i]) > 0)
                poEntry = poNode->GetNamedChild(pszAuxMetaData[i]);
            else
                poEntry = poNode;

            if (poEntry == nullptr && strlen(pszAuxMetaData[i + 3]) > 0)
            {
                // Child does not yet exist --> create it,
                poEntry = MMREntry::New(hMMR, pszAuxMetaData[i],
                                        pszAuxMetaData[i + 3], poNode);

                if (STARTS_WITH_CI(pszAuxMetaData[i], "Statistics"))
                    bCreatedStatistics = true;

                if (STARTS_WITH_CI(pszAuxMetaData[i], "HistogramParameters"))
                {
                    // A bit nasty.  Need to set the string field for the object
                    // first because the SetStringField sets the count for the
                    // object BinFunction to the length of the string.
                    poEntry->MakeData(70);
                    poEntry->SetStringField("BinFunction.binFunctionType",
                                            "direct");

                    bCreatedHistogramParameters = true;
                }
            }
            if (poEntry == nullptr)
            {
                CPLFree(pszKey);
                continue;
            }

            const char *pszFieldName = pszAuxMetaData[i + 1] + 1;
            switch (pszAuxMetaData[i + 1][0])
            {
                case 'd':
                {
                    double dfValue = CPLAtof(pszValue);
                    poEntry->SetDoubleField(pszFieldName, dfValue);
                }
                break;
                case 'i':
                case 'l':
                {
                    int nValue = atoi(pszValue);
                    poEntry->SetIntField(pszFieldName, nValue);
                }
                break;
                case 's':
                case 'e':
                {
                    poEntry->SetStringField(pszFieldName, pszValue);
                }
                break;
                default:
                    CPLAssert(false);
            }
        }
        else if (STARTS_WITH_CI(pszKey, "STATISTICS_HISTOBINVALUES"))
        {
            CPLFree(pszBinValues);
            pszBinValues = CPLStrdup(pszValue);
        }
        else
        {
            papszGDALMD = CSLAddString(papszGDALMD, papszMD[iColumn]);
        }

        CPLFree(pszKey);
    }

    // Special case to write out the histogram.
    bool bRet = true;
    if (pszBinValues != nullptr)
    {
        MMREntry *poEntry = poNode->GetNamedChild("HistogramParameters");
        if (poEntry != nullptr && bCreatedHistogramParameters)
        {
            // If this node exists we have added Histogram data -- complete with
            // some defaults.
            poEntry->SetIntField("SkipFactorX", 1);
            poEntry->SetIntField("SkipFactorY", 1);

            const int nNumBins = poEntry->GetIntField("BinFunction.numBins");
            const double dMinLimit =
                poEntry->GetDoubleField("BinFunction.minLimit");
            const double dMaxLimit =
                poEntry->GetDoubleField("BinFunction.maxLimit");

            // Fill the descriptor table - check it isn't there already.
            poEntry = poNode->GetNamedChild("Descriptor_Table");
            if (poEntry == nullptr || !EQUAL(poEntry->GetType(), "Edsc_Table"))
                poEntry = MMREntry::New(hMMR, "Descriptor_Table", "Edsc_Table",
                                        poNode);

            poEntry->SetIntField("numRows", nNumBins);

            // Bin function.
            MMREntry *poBinFunc = poEntry->GetNamedChild("#Bin_Function#");
            if (poBinFunc == nullptr ||
                !EQUAL(poBinFunc->GetType(), "Edsc_BinFunction"))
                poBinFunc = MMREntry::New(hMMR, "#Bin_Function#",
                                          "Edsc_BinFunction", poEntry);

            poBinFunc->MakeData(30);
            poBinFunc->SetIntField("numBins", nNumBins);
            poBinFunc->SetDoubleField("minLimit", dMinLimit);
            poBinFunc->SetDoubleField("maxLimit", dMaxLimit);
            // Direct for thematic layers, linear otherwise.
            if (STARTS_WITH_CI(poNode->GetStringField("layerType"), "thematic"))
                poBinFunc->SetStringField("binFunctionType", "direct");
            else
                poBinFunc->SetStringField("binFunctionType", "linear");

            // We need a child named histogram.
            MMREntry *poHisto = poEntry->GetNamedChild("Histogram");
            if (poHisto == nullptr || !EQUAL(poHisto->GetType(), "Edsc_Column"))
                poHisto =
                    MMREntry::New(hMMR, "Histogram", "Edsc_Column", poEntry);

            poHisto->SetIntField("numRows", nNumBins);
            // Allocate space for the bin values.
            GUInt32 nOffset = MMRAllocateSpace(hMMR, nNumBins * 8);
            poHisto->SetIntField("columnDataPtr", nOffset);
            poHisto->SetStringField("dataType", "real");
            poHisto->SetIntField("maxNumChars", 0);
            // Write out histogram data.
            char *pszWork = pszBinValues;
            for (int nBin = 0; nBin < nNumBins; ++nBin)
            {
                char *pszEnd = strchr(pszWork, '|');
                if (pszEnd != nullptr)
                {
                    *pszEnd = 0;
                    bRet &=
                        VSIFSeekL(hMMR->fp, nOffset + 8 * nBin, SEEK_SET) >= 0;
                    double nValue = CPLAtof(pszWork);
                    MMRStandard(8, &nValue);

                    bRet &= VSIFWriteL((void *)&nValue, 8, 1, hMMR->fp) > 0;
                    pszWork = pszEnd + 1;
                }
            }
        }
        else if (poEntry != nullptr)
        {
            // In this case, there are HistogramParameters present, but we did
            // not create them. However, we might be modifying them, in the case
            // where the data has changed and the histogram counts need to be
            // updated. It could be worse than that, but that is all we are
            // going to cope with for now.  We are assuming that we did not
            // change any of the other stuff, like skip factors and so
            // forth. The main need for this case is for programs (such as
            // Imagine itself) which will happily modify the pixel values
            // without re-calculating the histogram counts.
            int nNumBins = poEntry->GetIntField("BinFunction.numBins");
            MMREntry *poEntryDescrTbl =
                poNode->GetNamedChild("Descriptor_Table");
            MMREntry *poHisto = nullptr;
            if (poEntryDescrTbl != nullptr)
            {
                poHisto = poEntryDescrTbl->GetNamedChild("Histogram");
            }
            if (poHisto != nullptr)
            {
                int nOffset = poHisto->GetIntField("columnDataPtr");
                // Write out histogram data.
                char *pszWork = pszBinValues;

                // Check whether histogram counts were written as int or double
                bool bCountIsInt = true;
                const char *pszDataType = poHisto->GetStringField("dataType");
                if (STARTS_WITH_CI(pszDataType, "real"))
                {
                    bCountIsInt = false;
                }
                for (int nBin = 0; nBin < nNumBins; ++nBin)
                {
                    char *pszEnd = strchr(pszWork, '|');
                    if (pszEnd != nullptr)
                    {
                        *pszEnd = 0;
                        if (bCountIsInt)
                        {
                            // Histogram counts were written as ints, so
                            // re-write them the same way.
                            bRet &= VSIFSeekL(hMMR->fp, nOffset + 4 * nBin,
                                              SEEK_SET) >= 0;
                            int nValue = atoi(pszWork);
                            MMRStandard(4, &nValue);
                            bRet &=
                                VSIFWriteL((void *)&nValue, 4, 1, hMMR->fp) > 0;
                        }
                        else
                        {
                            // Histogram were written as doubles, as is now the
                            // default behavior.
                            bRet &= VSIFSeekL(hMMR->fp, nOffset + 8 * nBin,
                                              SEEK_SET) >= 0;
                            double nValue = CPLAtof(pszWork);
                            MMRStandard(8, &nValue);
                            bRet &=
                                VSIFWriteL((void *)&nValue, 8, 1, hMMR->fp) > 0;
                        }
                        pszWork = pszEnd + 1;
                    }
                }
            }
        }
        CPLFree(pszBinValues);
    }

    // If we created a statistics node then try to create a
    // StatisticsParameters node too.
    if (bCreatedStatistics)
    {
        MMREntry *poEntry =
            MMREntry::New(hMMR, "StatisticsParameters",
                          "Eimg_StatisticsParameters830", poNode);

        poEntry->MakeData(70);
        // poEntry->SetStringField( "BinFunction.binFunctionType", "linear" );

        poEntry->SetIntField("SkipFactorX", 1);
        poEntry->SetIntField("SkipFactorY", 1);
    }

    // Write out metadata items without a special place.
    if (bRet && CSLCount(papszGDALMD) != 0)
    {
        CPLErr eErr = MMRSetGDALMetadata(hMMR, nBand, papszGDALMD);

        CSLDestroy(papszGDALMD);
        return eErr;
    }
    else
    {
        CSLDestroy(papszGDALMD);
        return CE_Failure;
    }
}

/************************************************************************/
/*                       MMREvaluateXFormStack()                        */
/************************************************************************/

int MMREvaluateXFormStack(int nStepCount, int bForward,
                          Efga_Polynomial *pasPolyList, double *pdfX,
                          double *pdfY)

{
    for (int iStep = 0; iStep < nStepCount; iStep++)
    {
        const Efga_Polynomial *psStep =
            bForward ? pasPolyList + iStep
                     : pasPolyList + nStepCount - iStep - 1;

        if (psStep->order == 1)
        {
            const double dfXOut = psStep->polycoefvector[0] +
                                  psStep->polycoefmtx[0] * *pdfX +
                                  psStep->polycoefmtx[2] * *pdfY;

            const double dfYOut = psStep->polycoefvector[1] +
                                  psStep->polycoefmtx[1] * *pdfX +
                                  psStep->polycoefmtx[3] * *pdfY;

            *pdfX = dfXOut;
            *pdfY = dfYOut;
        }
        else if (psStep->order == 2)
        {
            const double dfXOut = psStep->polycoefvector[0] +
                                  psStep->polycoefmtx[0] * *pdfX +
                                  psStep->polycoefmtx[2] * *pdfY +
                                  psStep->polycoefmtx[4] * *pdfX * *pdfX +
                                  psStep->polycoefmtx[6] * *pdfX * *pdfY +
                                  psStep->polycoefmtx[8] * *pdfY * *pdfY;
            const double dfYOut = psStep->polycoefvector[1] +
                                  psStep->polycoefmtx[1] * *pdfX +
                                  psStep->polycoefmtx[3] * *pdfY +
                                  psStep->polycoefmtx[5] * *pdfX * *pdfX +
                                  psStep->polycoefmtx[7] * *pdfX * *pdfY +
                                  psStep->polycoefmtx[9] * *pdfY * *pdfY;

            *pdfX = dfXOut;
            *pdfY = dfYOut;
        }
        else if (psStep->order == 3)
        {
            const double dfXOut =
                psStep->polycoefvector[0] + psStep->polycoefmtx[0] * *pdfX +
                psStep->polycoefmtx[2] * *pdfY +
                psStep->polycoefmtx[4] * *pdfX * *pdfX +
                psStep->polycoefmtx[6] * *pdfX * *pdfY +
                psStep->polycoefmtx[8] * *pdfY * *pdfY +
                psStep->polycoefmtx[10] * *pdfX * *pdfX * *pdfX +
                psStep->polycoefmtx[12] * *pdfX * *pdfX * *pdfY +
                psStep->polycoefmtx[14] * *pdfX * *pdfY * *pdfY +
                psStep->polycoefmtx[16] * *pdfY * *pdfY * *pdfY;
            const double dfYOut =
                psStep->polycoefvector[1] + psStep->polycoefmtx[1] * *pdfX +
                psStep->polycoefmtx[3] * *pdfY +
                psStep->polycoefmtx[5] * *pdfX * *pdfX +
                psStep->polycoefmtx[7] * *pdfX * *pdfY +
                psStep->polycoefmtx[9] * *pdfY * *pdfY +
                psStep->polycoefmtx[11] * *pdfX * *pdfX * *pdfX +
                psStep->polycoefmtx[13] * *pdfX * *pdfX * *pdfY +
                psStep->polycoefmtx[15] * *pdfX * *pdfY * *pdfY +
                psStep->polycoefmtx[17] * *pdfY * *pdfY * *pdfY;

            *pdfX = dfXOut;
            *pdfY = dfYOut;
        }
        else
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                         MMRWriteXFormStack()                         */
/************************************************************************/

CPLErr MMRWriteXFormStack(MMRHandle hMMR, int nBand, int nXFormCount,
                          Efga_Polynomial **ppasPolyListForward,
                          Efga_Polynomial **ppasPolyListReverse)

{
    if (nXFormCount == 0)
        return CE_None;

    if (ppasPolyListForward[0]->order != 1)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "For now MMRWriteXFormStack() only supports order 1 polynomials");
        return CE_Failure;
    }

    if (nBand < 0 || nBand > hMMR->nBands)
        return CE_Failure;

    // If no band number is provided, operate on all bands.
    if (nBand == 0)
    {
        for (nBand = 1; nBand <= hMMR->nBands; nBand++)
        {
            CPLErr eErr =
                MMRWriteXFormStack(hMMR, nBand, nXFormCount,
                                   ppasPolyListForward, ppasPolyListReverse);
            if (eErr != CE_None)
                return eErr;
        }

        return CE_None;
    }

    // Fetch our band node.
    MMREntry *poBandNode = hMMR->papoBand[nBand - 1]->poNode;
    MMREntry *poXFormHeader = poBandNode->GetNamedChild("MapToPixelXForm");
    if (poXFormHeader == nullptr)
    {
        poXFormHeader = MMREntry::New(hMMR, "MapToPixelXForm",
                                      "Exfr_GenericXFormHeader", poBandNode);
        poXFormHeader->MakeData(23);
        poXFormHeader->SetPosition();
        poXFormHeader->SetStringField("titleList.string", "Affine");
    }

    // Loop over XForms.
    for (int iXForm = 0; iXForm < nXFormCount; iXForm++)
    {
        Efga_Polynomial *psForward = *ppasPolyListForward + iXForm;
        CPLString osXFormName;
        osXFormName.Printf("XForm%d", iXForm);

        MMREntry *poXForm = poXFormHeader->GetNamedChild(osXFormName);

        if (poXForm == nullptr)
        {
            poXForm = MMREntry::New(hMMR, osXFormName, "Efga_Polynomial",
                                    poXFormHeader);
            poXForm->MakeData(136);
            poXForm->SetPosition();
        }

        poXForm->SetIntField("order", 1);
        poXForm->SetIntField("numdimtransform", 2);
        poXForm->SetIntField("numdimpolynomial", 2);
        poXForm->SetIntField("termcount", 3);
        poXForm->SetIntField("exponentlist[0]", 0);
        poXForm->SetIntField("exponentlist[1]", 0);
        poXForm->SetIntField("exponentlist[2]", 1);
        poXForm->SetIntField("exponentlist[3]", 0);
        poXForm->SetIntField("exponentlist[4]", 0);
        poXForm->SetIntField("exponentlist[5]", 1);

        poXForm->SetIntField("polycoefmtx[-3]", EPT_f64);
        poXForm->SetIntField("polycoefmtx[-2]", 2);
        poXForm->SetIntField("polycoefmtx[-1]", 2);
        poXForm->SetDoubleField("polycoefmtx[0]", psForward->polycoefmtx[0]);
        poXForm->SetDoubleField("polycoefmtx[1]", psForward->polycoefmtx[1]);
        poXForm->SetDoubleField("polycoefmtx[2]", psForward->polycoefmtx[2]);
        poXForm->SetDoubleField("polycoefmtx[3]", psForward->polycoefmtx[3]);

        poXForm->SetIntField("polycoefvector[-3]", EPT_f64);
        poXForm->SetIntField("polycoefvector[-2]", 1);
        poXForm->SetIntField("polycoefvector[-1]", 2);
        poXForm->SetDoubleField("polycoefvector[0]",
                                psForward->polycoefvector[0]);
        poXForm->SetDoubleField("polycoefvector[1]",
                                psForward->polycoefvector[1]);
    }

    return CE_None;
}

/************************************************************************/
/*                         MMRReadElevationUnit()                       */
/************************************************************************/

const char *MMRReadElevationUnit(MMRHandle hMMR, int iBand)
{
    if (hMMR->nBands <= iBand)
        return nullptr;

    MMRBand *poBand(hMMR->papoBand[iBand]);
    if (poBand == nullptr || poBand->poNode == nullptr)
    {
        return nullptr;
    }
    MMREntry *poElevInfo = poBand->poNode->GetNamedChild("Elevation_Info");
    if (poElevInfo == nullptr)
    {
        return nullptr;
    }
    return poElevInfo->GetStringField("elevationUnit");
}

/************************************************************************/
/*                         MMRSetGeoTransform()                         */
/*                                                                      */
/*      Set a MapInformation and XForm block.  Allows for rotated       */
/*      and shared geotransforms.                                       */
/************************************************************************/

CPLErr MMRSetGeoTransform(MMRHandle hMMR, const char *pszProName,
                          const char *pszUnits, double *padfGeoTransform)

{
    // Write MapInformation.
    for (int nBand = 1; nBand <= hMMR->nBands; nBand++)
    {
        MMREntry *poBandNode = hMMR->papoBand[nBand - 1]->poNode;

        MMREntry *poMI = poBandNode->GetNamedChild("MapInformation");
        if (poMI == nullptr)
        {
            poMI = MMREntry::New(hMMR, "MapInformation", "Eimg_MapInformation",
                                 poBandNode);
            poMI->MakeData(
                static_cast<int>(18 + strlen(pszProName) + strlen(pszUnits)));
            poMI->SetPosition();
        }

        poMI->SetStringField("projection.string", pszProName);
        poMI->SetStringField("units.string", pszUnits);
    }

    // Write XForm.
    double adfAdjTransform[6] = {};

    // Offset by half pixel.

    memcpy(adfAdjTransform, padfGeoTransform, sizeof(double) * 6);
    adfAdjTransform[0] += adfAdjTransform[1] * 0.5;
    adfAdjTransform[0] += adfAdjTransform[2] * 0.5;
    adfAdjTransform[3] += adfAdjTransform[4] * 0.5;
    adfAdjTransform[3] += adfAdjTransform[5] * 0.5;

    // Invert.
    double adfRevTransform[6] = {};
    if (!MMRInvGeoTransform(adfAdjTransform, adfRevTransform))
        memset(adfRevTransform, 0, sizeof(adfRevTransform));

    // Assign to polynomial object.

    Efga_Polynomial sForward;
    memset(&sForward, 0, sizeof(sForward));
    Efga_Polynomial *psForward = &sForward;
    sForward.order = 1;
    sForward.polycoefvector[0] = adfRevTransform[0];
    sForward.polycoefmtx[0] = adfRevTransform[1];
    sForward.polycoefmtx[1] = adfRevTransform[4];
    sForward.polycoefvector[1] = adfRevTransform[3];
    sForward.polycoefmtx[2] = adfRevTransform[2];
    sForward.polycoefmtx[3] = adfRevTransform[5];

    Efga_Polynomial sReverse = sForward;
    Efga_Polynomial *psReverse = &sReverse;

    return MMRWriteXFormStack(hMMR, 0, 1, &psForward, &psReverse);
}

/************************************************************************/
/*                        MMRRenameReferences()                         */
/*                                                                      */
/*      Rename references in this .img file from the old basename to    */
/*      a new basename.  This should be passed on to .aux and .rrd      */
/*      files and should include references to .aux, .rrd and .ige.     */
/************************************************************************/

CPLErr MMRRenameReferences(MMRHandle hMMR, const char *pszNewBase,
                           const char *pszOldBase)

{
    // Handle RRDNamesList updates.
    std::vector<MMREntry *> apoNodeList =
        hMMR->poRoot->FindChildren("RRDNamesList", nullptr);

    for (size_t iNode = 0; iNode < apoNodeList.size(); iNode++)
    {
        MMREntry *poRRDNL = apoNodeList[iNode];
        std::vector<CPLString> aosNL;

        // Collect all the existing names.
        const int nNameCount = poRRDNL->GetFieldCount("nameList");

        CPLString osAlgorithm = poRRDNL->GetStringField("algorithm.string");
        for (int i = 0; i < nNameCount; i++)
        {
            CPLString osFN;
            osFN.Printf("nameList[%d].string", i);
            aosNL.push_back(poRRDNL->GetStringField(osFN));
        }

        // Adjust the names to the new form.
        for (int i = 0; i < nNameCount; i++)
        {
            if (strncmp(aosNL[i], pszOldBase, strlen(pszOldBase)) == 0)
            {
                std::string osNew = pszNewBase;
                osNew += aosNL[i].c_str() + strlen(pszOldBase);
                aosNL[i] = std::move(osNew);
            }
        }

        // Try to make sure the RRDNamesList is big enough to hold the
        // adjusted name list.
        if (strlen(pszNewBase) > strlen(pszOldBase))
        {
            CPLDebug("MiraMonRaster", "Growing RRDNamesList to hold new names");
            poRRDNL->MakeData(static_cast<int>(
                poRRDNL->GetDataSize() +
                nNameCount * (strlen(pszNewBase) - strlen(pszOldBase))));
        }

        // Initialize the whole thing to zeros for a clean start.
        memset(poRRDNL->GetData(), 0, poRRDNL->GetDataSize());

        // Write the updates back to the file.
        poRRDNL->SetStringField("algorithm.string", osAlgorithm);
        for (int i = 0; i < nNameCount; i++)
        {
            CPLString osFN;
            osFN.Printf("nameList[%d].string", i);
            poRRDNL->SetStringField(osFN, aosNL[i]);
        }
    }

    // Spill file references.
    apoNodeList =
        hMMR->poRoot->FindChildren("ExternalRasterDMS", "ImgExternalRaster");

    for (size_t iNode = 0; iNode < apoNodeList.size(); iNode++)
    {
        MMREntry *poERDMS = apoNodeList[iNode];

        if (poERDMS == nullptr)
            continue;

        // Fetch all existing values.
        CPLString osFileName = poERDMS->GetStringField("fileName.string");

        GInt32 anValidFlagsOffset[2] = {
            poERDMS->GetIntField("layerStackValidFlagsOffset[0]"),
            poERDMS->GetIntField("layerStackValidFlagsOffset[1]")};

        GInt32 anStackDataOffset[2] = {
            poERDMS->GetIntField("layerStackDataOffset[0]"),
            poERDMS->GetIntField("layerStackDataOffset[1]")};

        const GInt32 nStackCount = poERDMS->GetIntField("layerStackCount");
        const GInt32 nStackIndex = poERDMS->GetIntField("layerStackIndex");

        // Update the filename.
        if (strncmp(osFileName, pszOldBase, strlen(pszOldBase)) == 0)
        {
            std::string osNew = pszNewBase;
            osNew += osFileName.c_str() + strlen(pszOldBase);
            osFileName = std::move(osNew);
        }

        // Grow the node if needed.
        if (strlen(pszNewBase) > strlen(pszOldBase))
        {
            CPLDebug("MiraMonRaster",
                     "Growing ExternalRasterDMS to hold new names");
            poERDMS->MakeData(
                static_cast<int>(poERDMS->GetDataSize() +
                                 (strlen(pszNewBase) - strlen(pszOldBase))));
        }

        // Initialize the whole thing to zeros for a clean start.
        memset(poERDMS->GetData(), 0, poERDMS->GetDataSize());

        // Write it all out again, this may change the size of the node.
        poERDMS->SetStringField("fileName.string", osFileName);
        poERDMS->SetIntField("layerStackValidFlagsOffset[0]",
                             anValidFlagsOffset[0]);
        poERDMS->SetIntField("layerStackValidFlagsOffset[1]",
                             anValidFlagsOffset[1]);

        poERDMS->SetIntField("layerStackDataOffset[0]", anStackDataOffset[0]);
        poERDMS->SetIntField("layerStackDataOffset[1]", anStackDataOffset[1]);

        poERDMS->SetIntField("layerStackCount", nStackCount);
        poERDMS->SetIntField("layerStackIndex", nStackIndex);
    }

    // DependentFile.
    apoNodeList =
        hMMR->poRoot->FindChildren("DependentFile", "Eimg_DependentFile");

    for (size_t iNode = 0; iNode < apoNodeList.size(); iNode++)
    {
        CPLString osFileName =
            apoNodeList[iNode]->GetStringField("dependent.string");

        // Grow the node if needed.
        if (strlen(pszNewBase) > strlen(pszOldBase))
        {
            CPLDebug("MiraMonRaster",
                     "Growing DependentFile to hold new names");
            apoNodeList[iNode]->MakeData(
                static_cast<int>(apoNodeList[iNode]->GetDataSize() +
                                 (strlen(pszNewBase) - strlen(pszOldBase))));
        }

        // Update the filename.
        if (strncmp(osFileName, pszOldBase, strlen(pszOldBase)) == 0)
        {
            std::string osNew = pszNewBase;
            osNew += (osFileName.c_str() + strlen(pszOldBase));
            osFileName = std::move(osNew);
        }

        apoNodeList[iNode]->SetStringField("dependent.string", osFileName);
    }

    return CE_None;
}
