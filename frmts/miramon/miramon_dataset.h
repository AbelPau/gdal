/******************************************************************************
 *
 * Project:  MiraMon Raster Driver
 * Purpose:  Implements MMRDataset and MMRRasterBand class 
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
#include <optional>

#include "gdal_pam.h"
#include "gdal_rat.h"
#include "miramon_rel.h"  // For MMRInfo

/************************************************************************/
/* ==================================================================== */
/*                              MMRDataset                              */
/* ==================================================================== */
/************************************************************************/

class MMRRasterBand;

class MMRDataset final : public GDALPamDataset
{
  public:
    explicit MMRDataset(GDALOpenInfo *poOpenInfo);
    MMRDataset(const MMRDataset &) =
        delete;  // I don't want to construct a MMRDataset from another MMRDataset (effc++)
    MMRDataset &operator=(const MMRDataset &) =
        delete;  // I don't want to assing a MMRDataset to another MMRDataset (effc++)
    virtual ~MMRDataset();

    static int Identify(GDALOpenInfo *);
    static GDALDataset *Open(GDALOpenInfo *);

    void AssignBandsToSubdataSets();
    void CreateSubdatasetsFromBands();
    void AssignBands();

    int GetDataSetBoundingBox();
    int GetBandBoundingBox(int nIBand);

    MMRInfo *GetMMRInfo()
    {
        return hMMR;
    }

    const OGRSpatialReference *GetSpatialRef() const override;
    virtual CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    virtual CPLErr SetMetadata(char **, const char * = "") override;
    virtual CPLErr SetMetadataItem(const char *, const char *,
                                   const char * = "") override;

  private:
    GDALGeoTransform m_gt{};
    OGRSpatialReference m_oSRS{};
    CPLErr ReadProjection();

    bool NextBandInANewDataSet(int nIBand);

    MMRInfo *hMMR = nullptr;  //owner

    bool bMetadataDirty = false;
    std::vector<gdal::GCP> m_aoGCPs{};

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

    void SetDataType();
    CPLErr FillRATFromDBF();
    CPLErr GetAttributeTableName(char *papszToken, CPLString &osRELName,
                                 CPLString &osDBFName,
                                 CPLString &osAssociateREL);
    CPLErr CreateAttributteTableFromDBF(CPLString osRELName,
                                        CPLString osDBFName,
                                        CPLString osAssociateRel);
};

#endif  // MMRDATASET_H_INCLUDED
