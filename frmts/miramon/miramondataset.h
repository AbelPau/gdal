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
    CPLErr WriteProjection();
    bool bForceToPEString = false;
    bool bDisablePEString = false;

    std::vector<gdal::GCP> m_aoGCPs{};

    void UseXFormStack(int nStepCount, Efga_Polynomial *pasPolyListForward,
                       Efga_Polynomial *pasPolyListReverse);

  protected:
    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, int, BANDMAP_TYPE,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;

  public:
    MMRDataset();
    virtual ~MMRDataset();

    static int Identify(GDALOpenInfo *);
    static CPLErr Rename(const char *pszNewName, const char *pszOldName);
    static CPLErr CopyFiles(const char *pszNewName, const char *pszOldName);
    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszParamList);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
    static CPLErr Delete(const char *pszFilename);

    //virtual char **GetFileList() override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual CPLErr GetGeoTransform(double *) override;
    virtual CPLErr SetGeoTransform(double *) override;

    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;

    virtual CPLErr SetMetadata(char **, const char * = "") override;
    virtual CPLErr SetMetadataItem(const char *, const char *,
                                   const char * = "") override;

    virtual CPLErr FlushCache(bool bAtClosing) override;
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

    void ReadAuxMetadata();
    void ReadHistogramMetadata();
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

class MMRAttributeField
{
  public:
    CPLString sName;
    GDALRATFieldType eType;
    GDALRATFieldUsage eUsage;
    int nDataOffset;
    int nElementSize;
    MMREntry *poColumn;
    bool bIsBinValues;    // Handled differently.
    bool bConvertColors;  // Map 0-1 floats to 0-255 ints.
};

class MMRRasterAttributeTable final : public GDALRasterAttributeTable
{
  private:
    MMRHandle hMMR;
    MMREntry *poDT;
    CPLString osName;
    int nBand;
    GDALAccess eAccess;

    std::vector<MMRAttributeField> aoFields;
    int nRows;

    bool bLinearBinning;
    double dfRow0Min;
    double dfBinSize;
    GDALRATTableType eTableType;

    CPLString osWorkingResult;

    void AddColumn(const char *pszName, GDALRATFieldType eType,
                   GDALRATFieldUsage eUsage, int nDataOffset, int nElementSize,
                   MMREntry *poColumn, bool bIsBinValues = false,
                   bool bConvertColors = false)
    {
        MMRAttributeField aField;
        aField.sName = pszName;
        aField.eType = eType;
        aField.eUsage = eUsage;
        aField.nDataOffset = nDataOffset;
        aField.nElementSize = nElementSize;
        aField.poColumn = poColumn;
        aField.bIsBinValues = bIsBinValues;
        aField.bConvertColors = bConvertColors;

        aoFields.push_back(aField);
    }

    void CreateDT()
    {
        poDT = MMREntry::New(hMMR->papoBand[nBand - 1]->psInfo, osName,
                             "Edsc_Table", hMMR->papoBand[nBand - 1]->poNode);
        poDT->SetIntField("numrows", nRows);
    }

  public:
    MMRRasterAttributeTable(MMRRasterBand *poBand, const char *pszName);
    virtual ~MMRRasterAttributeTable();

    GDALRasterAttributeTable *Clone() const override;

    virtual int GetColumnCount() const override;

    virtual const char *GetNameOfCol(int) const override;
    virtual GDALRATFieldUsage GetUsageOfCol(int) const override;
    virtual GDALRATFieldType GetTypeOfCol(int) const override;

    virtual int GetColOfUsage(GDALRATFieldUsage) const override;

    virtual int GetRowCount() const override;

    virtual const char *GetValueAsString(int iRow, int iField) const override;
    virtual int GetValueAsInt(int iRow, int iField) const override;
    virtual double GetValueAsDouble(int iRow, int iField) const override;

    virtual void SetValue(int iRow, int iField, const char *pszValue) override;
    virtual void SetValue(int iRow, int iField, double dfValue) override;
    virtual void SetValue(int iRow, int iField, int nValue) override;

    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, double *pdfData) override;
    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, int *pnData) override;
    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, char **papszStrList) override;

    virtual int ChangesAreWrittenToFile() override;
    virtual void SetRowCount(int iCount) override;

    virtual int GetRowOfValue(double dfValue) const override;
    virtual int GetRowOfValue(int nValue) const override;

    virtual CPLErr CreateColumn(const char *pszFieldName,
                                GDALRATFieldType eFieldType,
                                GDALRATFieldUsage eFieldUsage) override;
    virtual CPLErr SetLinearBinning(double dfRow0Min,
                                    double dfBinSize) override;
    virtual int GetLinearBinning(double *pdfRow0Min,
                                 double *pdfBinSize) const override;

    virtual CPLXMLNode *Serialize() const override;

    virtual CPLErr SetTableType(const GDALRATTableType eInTableType) override;
    virtual GDALRATTableType GetTableType() const override;
    virtual void RemoveStatistics() override;

  protected:
    CPLErr ColorsIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    int *pnData);
};

#endif  // MMRDATASET_H_INCLUDED
