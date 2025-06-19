/******************************************************************************
 *
 * Name:     miramondataset.h
 * Project:  MiraMon Raster Driver
 * Purpose:  Main driver for MiraMon Raster format.
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMRDATASET_H_INCLUDED
#define MMRDATASET_H_INCLUDED

#include <cstddef>
#include <vector>

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_rat.h"
#include "miramon_p.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"

/************************************************************************/
/* ==================================================================== */
/*                              MMRDataset                              */
/* ==================================================================== */
/************************************************************************/

class MMRRasterBand;

class MMRDataset final : public GDALPamDataset
{
    friend class MMRRasterBand;

    MMRHandle hMMR = nullptr;

    bool bMetadataDirty = false;
    bool bMonoBanda = true;  // The dataset is monobanda

    bool bGeoDirty = false;
    double adfGeoTransform[6];
    OGRSpatialReference m_oSRS{};

    bool bIgnoreUTM = false;

    CPLErr ReadProjection();
    bool bForceToPEString = false;
    bool bDisablePEString = false;

    std::vector<gdal::GCP> m_aoGCPs{};

    int GetColumnsNumberFromREL(int *nNCols);
    int GetRowsNumberFromREL(int *nNRows);

  public:
    MMRDataset();
    virtual ~MMRDataset();

    static int Identify(GDALOpenInfo *);
    static CPLErr CopyFiles(const char *pszNewName, const char *pszOldName);
    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *Create(const char *pszFileName, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszParamList);
    static GDALDataset *CreateCopy(const char *pszFileName,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
    static CPLErr Delete(const char *pszFileName);

    void AssignBandsToSubdataSets();
    void CreateSubdatasetsFromBands();
    void AssignBands(GDALOpenInfo *poOpenInfo);

    //virtual char **GetFileList() override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual CPLErr GetGeoTransform(double *) override;
    virtual CPLErr SetGeoTransform(double *) override;

    int GetDataSetBoundingBox();
    int GetBandBoundingBox(int nIBand);

    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;

    virtual CPLErr SetMetadata(char **, const char * = "") override;
    virtual CPLErr SetMetadataItem(const char *, const char *,
                                   const char * = "") override;

  private:
    bool NextBandInANewDataSet(int nIBand);

    // Numbers of subdatasets (if any) in this dataset.
    int nNSubdataSets;
};

/************************************************************************/
/* ==================================================================== */
/*                            MMRRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class MMRRasterBand final : public GDALPamRasterBand
{
    friend class MMRDataset;
    friend class MMRRasterAttributeTable;

    GDALColorTable *poCT;

    EPTType eMMRDataType;
    MMDataType eMMRDataTypeMiraMon;  // Arreglar nom
    MMBytesPerPixel eMMBytesPerPixel;

    MMRHandle hMMR;

    bool bMetadataDirty;

    GDALRasterAttributeTable *poDefaultRAT;

    CPLErr WriteNamedRAT(const char *pszName,
                         const GDALRasterAttributeTable *poRAT);

  public:
    MMRRasterBand(MMRDataset *, int);
    virtual ~MMRRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;

    virtual const char *GetDescription() const override;
    virtual void SetDescription(const char *) override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;
    virtual CPLErr SetColorTable(GDALColorTable *) override;

    virtual double GetMinimum(int *pbSuccess = nullptr) override;
    virtual double GetMaximum(int *pbSuccess = nullptr) override;
    virtual double GetNoDataValue(int *pbSuccess = nullptr) override;
    virtual CPLErr SetNoDataValue(double dfValue) override;

    virtual CPLErr SetMetadata(char **, const char * = "") override;
    virtual CPLErr SetMetadataItem(const char *, const char *,
                                   const char * = "") override;

    virtual CPLErr GetDefaultHistogram(double *pdfMin, double *pdfMax,
                                       int *pnBuckets, GUIntBig **ppanHistogram,
                                       int bForce, GDALProgressFunc,
                                       void *pProgressData) override;

    virtual GDALRasterAttributeTable *GetDefaultRAT() override;
    virtual CPLErr SetDefaultRAT(const GDALRasterAttributeTable *) override;
};

#endif  // MMRDATASET_H_INCLUDED
