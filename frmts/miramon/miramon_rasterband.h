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

#ifndef MMRRASTERBAND_H_INCLUDED
#define MMRRASTERBAND_H_INCLUDED

#include <cstddef>
#include <vector>
#include <optional>
#include <array>

#include "gdal_pam.h"
#include "gdal_rat.h"

#include "../miramon_common/mm_gdal_constants.h"  // For MM_EXT_DBF_N_FIELDS
#include "miramon_rel.h"                          // For MMDataType

/* ==================================================================== */
/*                            MMRRasterBand                             */
/* ==================================================================== */
class MMRRasterBand final : public GDALPamRasterBand
{
  public:
    MMRRasterBand(MMRDataset *, int);

    MMRRasterBand(const MMRRasterBand &) =
        delete;  // I don't want to construct a MMRRasterBand from another MMRRasterBand (effc++)
    MMRRasterBand &operator=(const MMRRasterBand &) =
        delete;  // I don't want to assing a MMRRasterBand to another MMRRasterBand (effc++)
    ~MMRRasterBand();

    CPLErr IReadBlock(int, int, void *) override;
    const char *GetDescription() const override;
    GDALColorInterp GetColorInterpretation() override;
    GDALColorTable *GetColorTable() override;
    double GetMinimum(int *pbSuccess = nullptr) override;
    double GetMaximum(int *pbSuccess = nullptr) override;
    double GetNoDataValue(int *pbSuccess = nullptr) override;
    CPLErr SetMetadata(char **, const char * = "") override;
    CPLErr SetMetadataItem(const char *, const char *,
                           const char * = "") override;
    GDALRasterAttributeTable *GetDefaultRAT() override;

    void SetDataType();
    CPLErr UpdateTableColorsFromPalette();
    CPLErr UpdateAttributeColorsFromPalette();
    void ConvertColorsFromPaletteToColorTable();
    CPLErr FillRATFromDBF();
    CPLErr GetAttributeTableName(char *papszToken, CPLString &osRELName,
                                 CPLString &osDBFName,
                                 CPLString &osAssociateREL);
    CPLErr CreateAttributteTableFromDBF(CPLString osRELName,
                                        CPLString osDBFName,
                                        CPLString osAssociateRel);

    char GetIsValid() const
    {
        return bIsValid;
    }

    void SetIsValid(bool bIsValidIn)
    {
        bIsValid = bIsValidIn;
    }

    const std::vector<double> &GetPCT_Red() const
    {
        return aadfPCT[0];
    }

    const std::vector<double> &GetPCT_Green() const
    {
        return aadfPCT[1];
    }

    const std::vector<double> &GetPCT_Blue() const
    {
        return aadfPCT[2];
    }

    const std::vector<double> &GetPCT_Alpha() const
    {
        return aadfPCT[3];
    }

    void SetMetadataDirty(bool isDirty)
    {
        bMetadataDirty = isDirty;
    }

  private:
    void AssignRGBColor(int nIndexDstPalete, int nIndexSrcPalete);
    void AssignRGBColorDirectly(int nIndexDstPalete, double dfValue);
    CPLErr FromPaletteToColorTableCategoricalMode();
    CPLErr FromPaletteToColorTableContinousMode();
    CPLErr FromPaletteToAttributeTableContinousMode();
    CPLErr FromPaletteToAttributeTableCategoricalMode();
    CPLErr AssignUniformColorTable();
    CPLErr ReadPalette(CPLString os_Color_Paleta_DBF);
    static CPLErr GetPaletteColors_DBF_Indexs(
        struct MM_DATA_BASE_XP &oColorTable, MM_EXT_DBF_N_FIELDS &nClauSimbol,
        MM_EXT_DBF_N_FIELDS &nRIndex, MM_EXT_DBF_N_FIELDS &nGIndex,
        MM_EXT_DBF_N_FIELDS &nBIndex);
    void AssignColorFromDBF(struct MM_DATA_BASE_XP &oColorTable,
                            char *pzsRecord, char *pzsField,
                            MM_EXT_DBF_N_FIELDS &nRIndex,
                            MM_EXT_DBF_N_FIELDS &nGIndex,
                            MM_EXT_DBF_N_FIELDS &nBIndex, int nIPaletteIndex);
    CPLErr GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF);
    CPLErr GetPaletteColors_PAL_P25_P65(CPLString os_Color_Paleta_DBF);

    bool bIsValid = false;  // Determines if the created object is valid or not.

    CPLString osBandSection = "";  // Name of the band

    MMDataType eMMRDataTypeMiraMon = MMDataType::DATATYPE_AND_COMPR_UNDEFINED;
    MMBytesPerPixel eMMBytesPerPixel =
        MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_UNDEFINED;

    MMRRel *pfRel = nullptr;  // Pointer to info from rel. Do not free.
    bool bMetadataDirty = false;

    GDALRasterAttributeTable *poDefaultRAT = nullptr;

    bool bPaletteColorsRead = false;
    bool bColorTableCategorical = false;
    bool bTriedLoadColorTable = false;
    GDALColorTable *poCT = nullptr;
    // Palette info
    std::array<std::vector<double>, 4> aadfPaletteColors{};
    GDALColorEntry sDefaultColorRGB = {0, 0, 0, 127};

    bool bPaletteHasNodata = false;
    // index in the DBF that gives nodata color
    int nNoDataPaletteIndex = 0;
    // Default color for nodata
    GDALColorEntry sNoDataColorRGB = {0, 0, 0, 0};

    std::array<std::vector<double>, 4> aadfPCT{};

    bool bConstantColor = false;
    GDALColorEntry sConstantColorRGB = {0, 0, 0, 0};
};

#endif  // MMRRASTERBAND_H_INCLUDED
