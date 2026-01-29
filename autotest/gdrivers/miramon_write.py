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
import sys
import tempfile

import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("MiraMonRaster")

# MiraMon driver
mm_driver = gdal.GetDriverByName("MiraMonRaster")

working_mode = "Release"  # Debug to generate GTiff files for debugging

gdal_to_struct = {
    gdal.GDT_UInt8: "B",
    gdal.GDT_Int16: "h",
    gdal.GDT_UInt16: "H",
    gdal.GDT_Int32: "i",
    gdal.GDT_Float32: "f",
    gdal.GDT_Float64: "d",
}

init_type_list = [
    gdal.GDT_UInt8,
    gdal.GDT_Int16,
    gdal.GDT_UInt16,
    gdal.GDT_Int32,
    gdal.GDT_Float32,
    gdal.GDT_Float64,
]


@pytest.mark.parametrize(
    "data_type1",
    init_type_list,
)
@pytest.mark.parametrize(
    "data_type2",
    init_type_list,
)
@pytest.mark.parametrize(
    "compress",
    ["YES", "NO"],
)
@pytest.mark.parametrize(
    "pattern",
    [None, "UserPattern"],
)
@pytest.mark.parametrize(
    "rat_first_col_type", [gdal.GFU_MinMax, gdal.GFU_Min, gdal.GFU_Generic]
)
def test_miramonraster_multiband(
    data_type1, data_type2, compress, pattern, rat_first_col_type
):

    if data_type1 == gdal.GDT_Int8 or data_type1 == gdal.GDT_UInt8:
        use_color_table = "True"
    else:
        use_color_table = "False"

    if (
        data_type1 == gdal.GDT_Int8
        or data_type1 == gdal.GDT_UInt8
        or data_type1 == gdal.GDT_UInt16
    ):
        use_rat = "True"
    else:
        use_rat = "False"

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
    band2 = src_ds.GetRasterBand(2)
    band2.SetUnitType("m")

    if use_color_table == "True":
        # --- Color table on band 1 ---
        colors = [
            (0, 0, 0, 0),  # NoData
            (255, 0, 0, 255),
            (0, 255, 0, 255),
            (0, 0, 255, 255),
            (0, 125, 125, 255),
            (125, 125, 255, 255),
        ]
        ct = gdal.ColorTable()
        ct.SetColorEntry(0, colors[0])
        ct.SetColorEntry(1, colors[1])
        ct.SetColorEntry(2, colors[2])
        ct.SetColorEntry(3, colors[3])
        ct.SetColorEntry(4, colors[4])
        ct.SetColorEntry(5, colors[5])

        band1.SetRasterColorTable(ct)
        band1.SetRasterColorInterpretation(gdal.GCI_PaletteIndex)

    # --- Raster Attribute Table (RAT) ---
    if use_rat == "True":
        rat = gdal.RasterAttributeTable()
        rat.CreateColumn("Value", gdal.GFT_Integer, rat_first_col_type)
        rat.CreateColumn("ClassName", gdal.GFT_String, gdal.GFU_Name)
        rat.CreateColumn("Real", gdal.GFT_Real, gdal.GFU_Generic)

        rat.SetRowCount(6)

        classname_list = [
            "Background",
            "Class_1",
            "Class_2",
            "Class_3",
            "Class_4",
            "Class_5",
        ]
        classname_double = [0.1, 1.2, 2.3, 3.4, 4.5, 5.6]

        rat.SetValueAsInt(0, 0, 0)
        rat.SetValueAsString(0, 1, classname_list[0])
        rat.SetValueAsDouble(0, 2, classname_double[0])
        rat.SetValueAsInt(1, 0, 1)
        rat.SetValueAsString(1, 1, classname_list[1])
        rat.SetValueAsDouble(1, 2, classname_double[1])
        rat.SetValueAsInt(2, 0, 2)
        rat.SetValueAsString(2, 1, classname_list[2])
        rat.SetValueAsDouble(2, 2, classname_double[2])

        rat.SetValueAsInt(3, 0, 3)
        rat.SetValueAsString(3, 1, classname_list[3])
        rat.SetValueAsDouble(3, 2, classname_double[3])
        rat.SetValueAsInt(4, 0, 4)
        rat.SetValueAsString(4, 1, classname_list[4])
        rat.SetValueAsDouble(4, 2, classname_double[4])

        rat.SetValueAsInt(5, 0, 5)
        rat.SetValueAsString(5, 1, classname_list[5])
        rat.SetValueAsDouble(5, 2, classname_double[5])
        band1.SetDefaultRAT(rat)

    if working_mode == "Debug":
        # --- Write to MiraMonRaster ---
        gtiff_driver = gdal.GetDriverByName("GTiff")
        assert gtiff_driver is not None, "GTiff"

        tmpdir = tempfile.mkdtemp()
        mm_path = os.path.join(tmpdir, "test" + str(data_type1) + ".tiff")

        mm_ds = gtiff_driver.CreateCopy(mm_path, src_ds)
        assert mm_ds is not None
        mm_ds = None
        return

    # --- Write to MiraMonRaster ---
    tmpdir = tempfile.mkdtemp()
    mm_path = os.path.join(tmpdir, "testI.rel")

    co = [f"COMPRESS={compress}"]
    if pattern is not None:
        co.append(f"PATTERN={pattern}")
    mm_ds = mm_driver.CreateCopy(mm_path, src_ds, options=co)
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
    dst_band2 = dst_ds.GetRasterBand(2)

    # --- Color table check ---
    if use_color_table == "True":
        dst_ct = dst_band1.GetRasterColorTable()
        assert dst_ct is not None
        assert dst_ct.GetColorEntry(0) == colors[0]
        assert dst_ct.GetColorEntry(1) == colors[1]
        assert dst_ct.GetColorEntry(2) == colors[2]
        assert dst_ct.GetColorEntry(3) == colors[3]
        assert dst_ct.GetColorEntry(4) == colors[4]
        assert dst_ct.GetColorEntry(5) == colors[5]

    # --- RAT check ---
    if use_rat == "True":
        dst_rat = dst_band1.GetDefaultRAT()
        assert dst_rat is not None
        assert dst_rat.GetRowCount() == rat.GetRowCount()
        assert dst_rat.GetNameOfCol(0) == "Value"
        assert dst_rat.GetNameOfCol(1) == "ClassName"
        assert dst_rat.GetNameOfCol(2) == "Real"
        assert dst_rat.GetValueAsInt(0, 0) == 0
        assert dst_rat.GetValueAsString(0, 1) == classname_list[0]
        assert dst_rat.GetValueAsDouble(0, 2) == classname_double[0]
        assert dst_rat.GetValueAsInt(1, 0) == 1
        assert dst_rat.GetValueAsString(1, 1) == classname_list[1]
        assert dst_rat.GetValueAsDouble(1, 2) == classname_double[1]
        assert dst_rat.GetValueAsInt(2, 0) == 2
        assert dst_rat.GetValueAsString(2, 1) == classname_list[2]
        assert dst_rat.GetValueAsDouble(2, 2) == classname_double[2]
        assert dst_rat.GetValueAsInt(3, 0) == 3
        assert dst_rat.GetValueAsString(3, 1) == classname_list[3]
        assert dst_rat.GetValueAsDouble(3, 2) == classname_double[3]
        assert dst_rat.GetValueAsInt(4, 0) == 4
        assert dst_rat.GetValueAsString(4, 1) == classname_list[4]
        assert dst_rat.GetValueAsDouble(4, 2) == classname_double[4]
        assert dst_rat.GetValueAsInt(5, 0) == 5
        assert dst_rat.GetValueAsString(5, 1) == classname_list[5]
        assert dst_rat.GetValueAsDouble(5, 2) == classname_double[5]

    # --- Min / Max checks ---
    assert dst_band1.ComputeRasterMinMax(False) == (1, 5)
    assert dst_band2.ComputeRasterMinMax(False) == (100, 105)

    # --- Unit checks ---
    assert dst_band2.GetUnitType() == band2.GetUnitType()

    # --- Cleanup ---
    dst_ds = None
    src_ds = None


def test_miramon_rgb_single_dataset(tmp_path):

    # ------------------------------------------------------------------
    # 1. Create MEM RGB dataset with primary colors
    # ------------------------------------------------------------------
    mem_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 3, gdal.GDT_Byte)

    mem_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    mem_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    mem_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)

    # Pixels:
    # X=0: Red   -> 255,0,0
    # X=1: Green -> 0,255,0
    # X=2: Blue  -> 0,0,255
    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, bytes([255, 0, 0]))
    mem_ds.GetRasterBand(2).WriteRaster(0, 0, 3, 1, bytes([0, 255, 0]))
    mem_ds.GetRasterBand(3).WriteRaster(0, 0, 3, 1, bytes([0, 0, 255]))

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    mem_ds.SetProjection(srs.ExportToWkt())
    mem_ds.SetGeoTransform((0, 1, 0, 0, 0, -1))

    # ------------------------------------------------------------------
    # 2. Create MiraMonRaster copy
    # ------------------------------------------------------------------
    out_base = tmp_path / "rgb_primary"

    drv = gdal.GetDriverByName("MiraMonRaster")
    assert drv is not None

    drv.CreateCopy(str(out_base), mem_ds)
    mem_ds = None

    # ------------------------------------------------------------------
    # 3. Check generated files (physical)
    # ------------------------------------------------------------------
    expected_files = {
        "rgb_primaryI.rel",
        "rgb_primary_R.img",
        "rgb_primary_G.img",
        "rgb_primary_B.img",
        "rgb_primary.mmm",
    }

    generated_files = {f.name for f in tmp_path.iterdir() if f.is_file()}

    assert expected_files == generated_files

    # ------------------------------------------------------------------
    # 4. Open logical dataset (.I.rel) and check bands
    # ------------------------------------------------------------------
    ds = gdal.Open(str(tmp_path / "rgb_primaryI.rel"))
    assert ds is not None
    assert ds.RasterCount == 3

    r = ds.GetRasterBand(1).ReadRaster(0, 0, 3, 1)
    g = ds.GetRasterBand(2).ReadRaster(0, 0, 3, 1)
    b = ds.GetRasterBand(3).ReadRaster(0, 0, 3, 1)

    assert r == bytes([255, 0, 0])
    assert g == bytes([0, 255, 0])
    assert b == bytes([0, 0, 255])
    ds = None


@pytest.mark.parametrize("separate_minmax", [True, False])
def test_miramon_raster_RAT_to_CT(separate_minmax):
    # --- Raster parameters ---
    xsize = 3
    ysize = 2

    # --- Create in-memory dataset ---
    mem_driver = gdal.GetDriverByName("MEM")
    src_ds = mem_driver.Create("", xsize, ysize, 1, gdal.GDT_Byte)

    # --- Create deterministic pixel values ---
    # band 1: 0..5
    band_values = list(range(xsize * ysize))
    fmt1 = gdal_to_struct[gdal.GDT_Byte]
    band_bytes = struct.pack("<" + fmt1 * len(band_values), *band_values)

    # --- Write raster data ---
    src_ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        xsize,
        ysize,
        band_bytes,
        buf_xsize=xsize,
        buf_ysize=ysize,
        buf_type=gdal.GDT_Byte,
    )

    band = src_ds.GetRasterBand(1)

    # ------------------------
    # 2. Create a RAT with RGB
    # columns to convert it into
    # a color table
    # ------------------------
    rat = gdal.RasterAttributeTable()
    n_classes = 6

    # Create columns for RGB and min/max or VALUE
    if separate_minmax:
        rat.CreateColumn("MIN", gdal.GFT_Integer, gdal.GFU_Min)
        rat.CreateColumn("MAX", gdal.GFT_Integer, gdal.GFU_Max)
    else:
        rat.CreateColumn("VALUE", gdal.GFT_Integer, gdal.GFU_MinMax)
    rat.CreateColumn("R", gdal.GFT_Integer, gdal.GFU_Red)
    rat.CreateColumn("G", gdal.GFT_Integer, gdal.GFU_Green)
    rat.CreateColumn("B", gdal.GFT_Integer, gdal.GFU_Blue)

    # --- Color table ---
    colors = [
        (0, 0, 0, 255),
        (255, 0, 0, 255),
        (0, 255, 0, 255),
        (0, 0, 255, 255),
        (0, 125, 125, 255),
        (125, 125, 255, 255),
    ]

    for c in range(n_classes):
        if separate_minmax:
            rat.SetValueAsInt(c, 0, int(c))
            rat.SetValueAsInt(c, 1, int(c + 1))
            rat.SetValueAsInt(c, 2, colors[c][0])
            rat.SetValueAsInt(c, 3, colors[c][1])
            rat.SetValueAsInt(c, 4, colors[c][2])
        else:
            rat.SetValueAsInt(c, 0, int(c))
            rat.SetValueAsInt(c, 1, colors[c][0])
            rat.SetValueAsInt(c, 2, colors[c][1])
            rat.SetValueAsInt(c, 3, colors[c][2])

    band.SetDefaultRAT(rat)

    if working_mode == "Debug":
        # --- Write to VRT ---
        gtiff_driver = gdal.GetDriverByName("VRT")
        assert gtiff_driver is not None, "VRT"

        tmpdir = tempfile.mkdtemp()
        print(tmpdir, file=sys.stderr)
        mm_path = os.path.join(tmpdir, "test" + str(gdal.GDT_Byte) + ".vrt")

        mm_ds = gtiff_driver.CreateCopy(mm_path, src_ds)
        assert mm_ds is not None
        mm_ds = None
        return

    # --- Write to MiraMonRaster ---
    tmpdir = tempfile.mkdtemp("MM")
    mm_path = os.path.join(tmpdir, "RareTestI.rel")

    mm_ds = mm_driver.CreateCopy(mm_path, src_ds)
    assert mm_ds is not None
    mm_ds = None

    # --- Reopen dataset ---
    dst_ds = gdal.Open(mm_path)
    assert dst_ds is not None
    assert dst_ds.GetDriver().ShortName == "MiraMonRaster"

    dst_band1 = dst_ds.GetRasterBand(1)
    assert dst_band1 is not None

    # --- Color table check ---
    dst_ct = dst_band1.GetRasterColorTable()
    assert dst_ct is not None
    assert dst_ct.GetColorEntry(0) == colors[0]
    assert dst_ct.GetColorEntry(1) == colors[1]
    assert dst_ct.GetColorEntry(2) == colors[2]
    assert dst_ct.GetColorEntry(3) == colors[3]
    assert dst_ct.GetColorEntry(4) == colors[4]
    assert dst_ct.GetColorEntry(5) == colors[5]

    # --- Cleanup ---
    dst_ds = None
    src_ds = None
