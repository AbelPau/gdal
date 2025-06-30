#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for all datatypes from a MiraMon file.
# Author:   Abel Pau <a.pau@creaf.cat>
#
###############################################################################
# Copyright (c) 2025, Abel Pau <a.pau@creaf.cat>
#
# SPDX-License-Identifier: MIT
###############################################################################


import struct

import pytest

from osgeo import gdal

gdal_to_struct = {
    gdal.GDT_Byte: ("B", 1),
    gdal.GDT_UInt16: ("H", 2),
    gdal.GDT_Int16: ("h", 2),
    gdal.GDT_UInt32: ("I", 4),
    gdal.GDT_Int32: ("i", 4),
    gdal.GDT_Float32: ("f", 4),
    gdal.GDT_Float64: ("d", 8),
}

init_list = [
    ("data/miramon/normal/byte_2x3_6_categs.img", 1),
    ("data/miramon/normal/byte_2x3_6_categsI.rel", 1),
    ("data/miramon/normal/integer_2x3_6_categs.img", 1),
    ("data/miramon/normal/integer_2x3_6_categsI.rel", 1),
    ("data/miramon/normal/uinteger_2x3_6_categs.img", 1),
    ("data/miramon/normal/uinteger_2x3_6_categsI.rel", 1),
    ("data/miramon/normal/long_2x3_6_categs.img", 1),
    ("data/miramon/normal/long_2x3_6_categsI.rel", 1),
    ("data/miramon/normal/real_2x3_6_categs.img", 1),
    ("data/miramon/normal/real_2x3_6_categsI.rel", 1),
    ("data/miramon/normal/double_2x3_6_categs.img", 1),
    ("data/miramon/normal/double_2x3_6_categsI.rel", 1),
    ("data/miramon/normal/byte_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/byte_2x3_6_categs_RLEI.rel", 1),
    ("data/miramon/normal/byte_2x3_6_categs_RLE_no_ind.img", 1),
    ("data/miramon/normal/byte_2x3_6_categs_RLE_no_indI.rel", 1),
    ("data/miramon/normal/integer_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/integer_2x3_6_categs_RLEI.rel", 1),
    ("data/miramon/normal/uinteger_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/uinteger_2x3_6_categs_RLEI.rel", 1),
    ("data/miramon/normal/long_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/long_2x3_6_categs_RLEI.rel", 1),
    ("data/miramon/normal/real_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/real_2x3_6_categs_RLEI.rel", 1),
    ("data/miramon/normal/double_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/double_2x3_6_categs_RLEI.rel", 1),
    ("data/miramon/normal/chess_bit.img", 1),
    ("data/miramon/normal/chess_bitI.rel", 1),
]


@pytest.mark.parametrize(
    "filename,band_idx",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.require_driver("MiraMonRaster")
def test_miramon_test_012345_raster(filename, band_idx):
    # ds = gdal.Open(filename)
    ds = gdal.OpenEx(filename, allowed_drivers=["MiraMonRaster"])
    assert ds is not None, "Could not open the file"
    band = ds.GetRasterBand(band_idx)
    assert band is not None, f"Error opening band {band_idx}"
    rchecksum = band.Checksum()

    if "chess" in filename:
        assert rchecksum == 32, f"Unexpected checksum: {rchecksum}"
    else:
        assert rchecksum == 15, f"Unexpected checksum: {rchecksum}"

    xsize = band.XSize
    ysize = band.YSize
    dtype = band.DataType
    assert dtype in gdal_to_struct, f"Unsupported GDAL data type: {dtype}"

    fmt, size = gdal_to_struct[dtype]
    buf = band.ReadRaster(0, 0, xsize, ysize, buf_type=dtype)
    assert buf is not None, "Could not read raster data"

    # unpack and assert values
    count = xsize * ysize
    values = struct.unpack(f"{count}{fmt}", buf)

    if "chess" in filename:  # testing a few values
        expected = [0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0]
        for i, exp in enumerate(expected):
            assert (
                values[i] == exp
            ), f"Unexpected pixel value at index {i}: got {values[i]}, expected {exp}"
    else:
        expected = [0, 1, 2, 3, 4, 5]
        for i, exp in enumerate(expected):
            assert i < len(values), f"Expected value at index {i}, but got fewer values"
            assert (
                values[i] == exp
            ), f"Unexpected pixel value at index {i}: got {values[i]}, expected {exp}"
