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
        "int8",
        gdal.GDT_UInt8,
        gdal.GDT_Int16,
        "True",
    ),
    (
        "int16",
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        "False",
    ),
    (
        "uint16",
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        "False",
    ),
    (
        "int32",
        gdal.GDT_Int32,
        gdal.GDT_Float32,
        "False",
    ),
    (
        "float32",
        gdal.GDT_Float32,
        gdal.GDT_UInt8,
        "False",
    ),
    (
        "float64",
        gdal.GDT_Float64,
        gdal.GDT_UInt8,
        "False",
    ),
]


@pytest.mark.parametrize(
    "dbg_name,data_type1,data_type2,use_color_table",
    init_list,
)
def test_miramonraster_multiband(dbg_name, data_type1, data_type2, use_color_table):
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

    # --- Color table on band 1 ---
    if use_color_table == "True":
        ct = gdal.ColorTable()
        ct.SetColorEntry(0, (0, 0, 0, 0))  # NoData
        ct.SetColorEntry(1, (255, 0, 0, 255))
        ct.SetColorEntry(2, (0, 255, 0, 255))
        ct.SetColorEntry(3, (0, 0, 255, 255))
        ct.SetColorEntry(4, (0, 125, 125, 255))
        ct.SetColorEntry(5, (125, 125, 255, 255))

        band1.SetRasterColorTable(ct)
        band1.SetRasterColorInterpretation(gdal.GCI_PaletteIndex)

    # --- Raster Attribute Table (RAT) ---
    rat = gdal.RasterAttributeTable()
    rat.CreateColumn("Value", gdal.GFT_Integer, gdal.GFU_MinMax)
    rat.CreateColumn("ClassName", gdal.GFT_String, gdal.GFU_Name)

    rat.SetRowCount(6)

    rat.SetValueAsInt(0, 0, 0)
    rat.SetValueAsString(0, 1, "Background")

    rat.SetValueAsInt(1, 0, 1)
    rat.SetValueAsString(1, 1, "Class_1")

    rat.SetValueAsInt(2, 0, 2)
    rat.SetValueAsString(2, 1, "Class_2")

    rat.SetValueAsInt(3, 0, 3)
    rat.SetValueAsString(3, 1, "Class_3")

    rat.SetValueAsInt(4, 0, 4)
    rat.SetValueAsString(4, 1, "Class_4")

    rat.SetValueAsInt(5, 0, 5)
    rat.SetValueAsString(5, 1, "Class_5")

    band1.SetDefaultRAT(rat)

    # --- Write to MiraMonRaster ---
    mm_driver = gdal.GetDriverByName("GTiff")
    assert mm_driver is not None, "GTiff driver not available"

    tmpdir = tempfile.mkdtemp()
    mm_path = os.path.join(tmpdir, "test" + dbg_name + ".tiff")

    mm_ds = mm_driver.CreateCopy(mm_path, src_ds)
    assert mm_ds is not None
    mm_ds = None
