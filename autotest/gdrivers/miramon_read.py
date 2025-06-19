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


# import gdaltest
import pytest

from osgeo import gdal

init_list = [
    ("data/miramon/normal/byte_2x3_6_categs.img", 1),
    ("data/miramon/normal/integer_2x3_6_categs.img", 1),
    ("data/miramon/normal/uinteger_2x3_6_categs.img", 1),
    ("data/miramon/normal/long_2x3_6_categs.img", 1),
    ("data/miramon/normal/real_2x3_6_categs.img", 1),
    ("data/miramon/normal/double_2x3_6_categs.img", 1),
    ("data/miramon/normal/byte_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/byte_2x3_6_categs_RLE_no_ind.img", 1),
    ("data/miramon/normal/integer_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/uinteger_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/long_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/real_2x3_6_categs_RLE.img", 1),
    ("data/miramon/normal/double_2x3_6_categs_RLE.img", 1),
]


@pytest.mark.parametrize(
    "filename,band_idx",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.require_driver("MiraMonRaster")
def test_miramon_test_012345_raster(filename, band_idx):
    ds = gdal.Open(filename)
    assert ds is not None, "Could not open the file"
    band = ds.GetRasterBand(band_idx)
    assert band is not None, f"Error opening band {band_idx}"
    rchecksum = band.Checksum()
    assert rchecksum == 15, f"Unexpected checksum: {15}"

    # Read the entire raster band as a NumPy array
    array = band.ReadAsArray()
    assert array is not None, "Could not read band as array"

    # Loop through all pixels in the array

    assert array[0][0] == 0, f"Unexpected pixel value: {0},{0},{0}"
    assert array[0][1] == 1, f"Unexpected pixel value: {0},{1},{1}"
    assert array[1][0] == 2, f"Unexpected pixel value: {1},{0},{2}"
    assert array[1][1] == 3, f"Unexpected pixel value: {1},{1},{3}"
    assert array[2][0] == 4, f"Unexpected pixel value: {2},{0},{4}"
    assert array[2][1] == 5, f"Unexpected pixel value: {2},{1},{5}"


@pytest.mark.require_driver("MiraMonRaster")
def test_miramon_test_bit_raster():
    ds = gdal.Open("data/miramon/normal/chess_bit.img")
    assert ds is not None, "Could not open the file"
    band = ds.GetRasterBand(1)
    assert band is not None, f"Error opening band {1}"

    # Read the entire raster band as a NumPy array
    array = band.ReadAsArray()
    assert array is not None, "Could not read band as array"

    # Loop through all pixels in the array

    for i in range(8):
        for j in range(8):
            expected = (i + j) % 2
            actual = array[i][j]
            assert actual == expected, f"Unexpected pixel value: {i},{j},{actual}"
