/******************************************************************************
 *
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

    MMRInfo *hMMR = nullptr;  //owner

    bool bMetadataDirty = false;

    GDALGeoTransform m_gt{};
    OGRSpatialReference m_oSRS{};

    bool bIgnoreUTM = false;

    CPLErr ReadProjection();
    bool bForceToPEString = false;
    bool bDisablePEString = false;

    std::vector<gdal::GCP> m_aoGCPs{};

    int GetColumnsNumberFromREL(int *nNCols);
    int GetRowsNumberFromREL(int *nNRows);

  public:
    explicit MMRDataset(GDALOpenInfo *poOpenInfo);
    MMRDataset(const MMRDataset &) =
        delete;  // I don't want to construct a MMRDataset from another MMRDataset (effc++)
    MMRDataset &operator=(const MMRDataset &) =
        delete;  // I don't want to assing a MMRDataset to another MMRDataset (effc++)
    virtual ~MMRDataset();

    static int Identify(GDALOpenInfo *);
    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *Create(const char *pszFileName, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszParamList);

    void AssignBandsToSubdataSets();
    void CreateSubdatasetsFromBands();
    void AssignBands(GDALOpenInfo *poOpenInfo);

    const OGRSpatialReference *GetSpatialRef() const override;

    virtual CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;

    int GetDataSetBoundingBox();
    int GetBandBoundingBox(int nIBand);

    virtual CPLErr SetMetadata(char **, const char * = "") override;
    virtual CPLErr SetMetadataItem(const char *, const char *,
                                   const char * = "") override;

  private:
    bool NextBandInANewDataSet(int nIBand);

    // Numbers of subdatasets (if any) in this dataset.
    int nNSubdataSets = 0;
};

/************************************************************************/
/* ==================================================================== */
/*                            MMRRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class MMRRasterBand final : public GDALPamRasterBand
{
    friend class MMRDataset;

    CPLString osBandSection;  // Name of the band

    GDALColorTable *poCT;

    MMDataType eMMRDataTypeMiraMon;  // Arreglar nom
    MMBytesPerPixel eMMBytesPerPixel;

    MMRInfo *hMMR = nullptr;  // Just a pointer. No need to be freed

    bool bMetadataDirty;

    GDALRasterAttributeTable *poDefaultRAT;

  public:
    MMRRasterBand(MMRDataset *, int);

    MMRRasterBand(const MMRRasterBand &) =
        delete;  // I don't want to construct a MMRRasterBand from another MMRRasterBand (effc++)
    MMRRasterBand &operator=(const MMRRasterBand &) =
        delete;  // I don't want to assing a MMRRasterBand to another MMRRasterBand (effc++)
    virtual ~MMRRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;

    virtual const char *GetDescription() const override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;

    virtual double GetMinimum(int *pbSuccess = nullptr) override;
    virtual double GetMaximum(int *pbSuccess = nullptr) override;
    virtual double GetNoDataValue(int *pbSuccess = nullptr) override;

    virtual CPLErr SetMetadata(char **, const char * = "") override;
    virtual CPLErr SetMetadataItem(const char *, const char *,
                                   const char * = "") override;

    virtual GDALRasterAttributeTable *GetDefaultRAT() override;

    CPLErr FillRATFromDBF();

    CPLErr GetAttributeTableName(char *papszToken, CPLString &osRELName,
                                 CPLString &osDBFName,
                                 CPLString &osAssociateREL);

    CPLErr CreateAttributteTableFromDBF(CPLString osRELName,
                                        CPLString osDBFName,
                                        CPLString osAssociateRel);
};

#endif  // MMRDATASET_H_INCLUDED
