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

#ifndef MMRATTRIBUTE_TABLE_H_INCLUDED
#define MMRATTRIBUTE_TABLE_H_INCLUDED

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
/*                          MMRAttributeField                           */
/* ==================================================================== */
/************************************************************************/

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

    virtual CPLErr SetValue(int iRow, int iField,
                            const char *pszValue) override;
    virtual CPLErr SetValue(int iRow, int iField, double dfValue) override;
    virtual CPLErr SetValue(int iRow, int iField, int nValue) override;

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

#endif  // MMRATTRIBUTE_TABLE_H_INCLUDED
