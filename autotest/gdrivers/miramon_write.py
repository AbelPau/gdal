#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic write support for to a MiraMon file.
# Author:   Abel Pau <a.pau@creaf.cat>
#
###############################################################################
# Copyright (c) 2025, Abel Pau <a.pau@creaf.cat>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import struct
import tempfile

import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("MiraMonRaster")


gdal_to_struct = {
    gdal.GDT_UInt8: "B",
    gdal.GDT_Int16: "h",
    gdal.GDT_UInt16: "H",
    gdal.GDT_Int32: "i",
    gdal.GDT_Float32: "f",
    gdal.GDT_Float64: "d",
}

init_list = [
    (
        gdal.GDT_UInt8,
        gdal.GDT_Int16,
        "True",
        "True",
    ),
    (
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        "False",
        "True",
    ),
    (
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        "False",
        "True",
    ),
    (
        gdal.GDT_Int32,
        gdal.GDT_Float32,
        "False",
        "False",
    ),
    (gdal.GDT_Float32, gdal.GDT_Float32, "False", "False"),
    (
        gdal.GDT_Float64,
        gdal.GDT_Float64,
        "False",
        "False",
    ),
]


@pytest.mark.parametrize(
    "data_type1,data_type2,use_color_table,use_rat",
    init_list,
)
def test_miramonraster_multiband(data_type1, data_type2, use_color_table, use_rat):

    gdal.AllRegister()

    # --- Raster parameters ---
    xsize = 3
    ysize = 2
    nbands = 2
    geotransform = (100.0, 10.0, 0.0, 200.0, 0.0, -10.0)

    srs = osr.SpatialReference()
    epsg_code = 25831
    srs.ImportFromEPSG(epsg_code)
    wkt = srs.ExportToWkt()

    # --- Create in-memory dataset ---
    mem_driver = gdal.GetDriverByName("MEM")
    src_ds = mem_driver.Create("", xsize, ysize, nbands, data_type1)

    src_ds.SetGeoTransform(geotransform)
    src_ds.SetProjection(wkt)

    # --- Create deterministic pixel values ---
    # band 1: 0..12
    # band 2: 100..112
    band1_values = list(range(xsize * ysize))
    band2_values = [v + 100 for v in band1_values]

    fmt1 = gdal_to_struct[data_type1]
    fmt2 = gdal_to_struct[data_type2]
    band1_bytes = struct.pack("<" + fmt1 * len(band1_values), *band1_values)
    band2_bytes = struct.pack("<" + fmt2 * len(band2_values), *band2_values)

    # --- Write raster data ---
    src_ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        xsize,
        ysize,
        band1_bytes,
        buf_xsize=xsize,
        buf_ysize=ysize,
        buf_type=data_type1,
    )

    src_ds.GetRasterBand(2).WriteRaster(
        0,
        0,
        xsize,
        ysize,
        band2_bytes,
        buf_xsize=xsize,
        buf_ysize=ysize,
        buf_type=data_type2,
    )

    for i in range(1, nbands + 1):
        band = src_ds.GetRasterBand(i)
        band.SetNoDataValue(0)
        band.FlushCache()

    band1 = src_ds.GetRasterBand(1)
    if use_color_table == "True":
        # --- Color table on band 1 ---
        color1 = (0, 0, 0, 0)  # NoData
        color2 = (255, 0, 0, 255)
        color3 = (0, 255, 0, 255)
        color4 = (0, 0, 255, 255)
        color5 = (0, 125, 125, 255)
        color6 = (125, 125, 255, 255)
        ct = gdal.ColorTable()
        ct.SetColorEntry(0, color1)
        ct.SetColorEntry(1, color2)
        ct.SetColorEntry(2, color3)
        ct.SetColorEntry(3, color4)
        ct.SetColorEntry(4, color5)
        ct.SetColorEntry(5, color6)

        band1.SetRasterColorTable(ct)
        band1.SetRasterColorInterpretation(gdal.GCI_PaletteIndex)

    # --- Raster Attribute Table (RAT) ---
    if use_rat == "True":
        rat = gdal.RasterAttributeTable()
        rat.CreateColumn("Value", gdal.GFT_Integer, gdal.GFU_MinMax)
        rat.CreateColumn("ClassName", gdal.GFT_String, gdal.GFU_Name)

        rat.SetRowCount(6)

        classname1 = "Background"
        classname2 = "Class_1"
        classname3 = "Class_2"
        classname4 = "Class_3"
        classname5 = "Class_4"
        classname6 = "Class_5"
        rat.SetValueAsInt(0, 0, 0)
        rat.SetValueAsString(0, 1, classname1)

        rat.SetValueAsInt(1, 0, 1)
        rat.SetValueAsString(1, 1, classname2)

        rat.SetValueAsInt(2, 0, 2)
        rat.SetValueAsString(2, 1, classname3)

        rat.SetValueAsInt(3, 0, 3)
        rat.SetValueAsString(3, 1, classname4)

        rat.SetValueAsInt(4, 0, 4)
        rat.SetValueAsString(4, 1, classname5)

        rat.SetValueAsInt(5, 0, 5)
        rat.SetValueAsString(5, 1, classname6)

        band1.SetDefaultRAT(rat)

    # --- Write to MiraMonRaster ---
    mm_driver = gdal.GetDriverByName("MiraMonRaster")
    assert mm_driver is not None, "MiraMonRaster driver not available"

    tmpdir = tempfile.mkdtemp()
    mm_path = os.path.join(tmpdir, "testI.rel")

    mm_ds = mm_driver.CreateCopy(mm_path, src_ds)
    assert mm_ds is not None
    mm_ds = None

    # --- Reopen dataset ---
    dst_ds = gdal.Open(mm_path)
    assert dst_ds is not None
    assert dst_ds.GetDriver().ShortName == "MiraMonRaster"

    # --- Dataset checks ---
    assert dst_ds.RasterXSize == xsize
    assert dst_ds.RasterYSize == ysize
    assert dst_ds.RasterCount == nbands
    assert dst_ds.GetGeoTransform() == geotransform

    # Comparing reference system
    if geotransform is not None:
        srs = dst_ds.GetSpatialRef()
        if (
            srs is not None
        ):  # in Fedora it returns None (but it's the only system it does)
            epsg_code = srs.GetAuthorityCode("PROJCS") or srs.GetAuthorityCode("GEOGCS")
            assert (
                epsg_code == epsg_code
            ), f"incorrect EPSG: {epsg_code}, waited {epsg_code}"

    # --- Pixel data checks ---
    dst_band1_bytes = dst_ds.GetRasterBand(1).ReadRaster(
        0, 0, xsize, ysize, buf_xsize=xsize, buf_ysize=ysize, buf_type=data_type1
    )

    dst_band2_bytes = dst_ds.GetRasterBand(2).ReadRaster(
        0, 0, xsize, ysize, buf_xsize=xsize, buf_ysize=ysize, buf_type=data_type2
    )

    assert dst_band1_bytes == band1_bytes
    assert dst_band2_bytes == band2_bytes

    dst_band1 = dst_ds.GetRasterBand(1)

    # --- Color table check ---
    if use_color_table == "True":
        dst_ct = dst_band1.GetRasterColorTable()
        assert dst_ct is not None
        assert dst_ct.GetColorEntry(0) == color1
        assert dst_ct.GetColorEntry(1) == color2
        assert dst_ct.GetColorEntry(2) == color3
        assert dst_ct.GetColorEntry(3) == color4
        assert dst_ct.GetColorEntry(4) == color5
        assert dst_ct.GetColorEntry(5) == color6

    # --- RAT check ---
    if use_rat == "True":
        dst_rat = dst_band1.GetDefaultRAT()
        assert dst_rat is not None
        assert dst_rat.GetRowCount() == rat.GetRowCount()
        assert dst_rat.GetNameOfCol(0) == "Value"
        assert dst_rat.GetNameOfCol(1) == "ClassName"
        assert dst_rat.GetValueAsInt(0, 0) == 0
        assert dst_rat.GetValueAsString(0, 1) == classname1
        assert dst_rat.GetValueAsInt(1, 0) == 1
        assert dst_rat.GetValueAsString(1, 1) == classname2
        assert dst_rat.GetValueAsInt(2, 0) == 2
        assert dst_rat.GetValueAsString(2, 1) == classname3
        assert dst_rat.GetValueAsInt(3, 0) == 3
        assert dst_rat.GetValueAsString(3, 1) == classname4
        assert dst_rat.GetValueAsInt(4, 0) == 4
        assert dst_rat.GetValueAsString(4, 1) == classname5
        assert dst_rat.GetValueAsInt(5, 0) == 5
        assert dst_rat.GetValueAsString(5, 1) == classname6

    # --- Min / Max checks ---
    assert dst_band1.ComputeRasterMinMax(False) == (1, 5)
    dst_band2 = dst_ds.GetRasterBand(2)
    assert dst_band2.ComputeRasterMinMax(False) == (100, 105)

    dst_ds = None
    src_ds = None
