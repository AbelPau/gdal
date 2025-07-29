.. _raster.miramon:

MiraMon Raster
==============

.. versionadded:: 3.12

.. shortname:: MiraMonRaster

.. built_in_by_default::

This driver is capable of reading raster files in the MiraMon format.

A `look-up table of MiraMon <https://www.miramon.cat/help/eng/mm32/AP6.htm>`__ and
`EPSG <https://epsg.org/home.html>`__ Spatial Reference Systems is used to match
identifiers between the two systems.

If a layer contains an old *.rel* format file (used in legacy datasets),
a warning will be issued explaining how to convert it into the modern *.rel 4* format.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Overview of MiraMon format
--------------------------

The MiraMon `.img` format is a binary raster format with rich metadata stored in a sidecar `.rel` file.
More information is available in the `public specification <https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf>`__.

By specifying either the name of the `.rel` metadata file or the name of any `.img` band file, the driver will automatically use the associated `.rel` file.
This metadata file governs how all bands are interpreted and accessed.

- **REL file**: Contains metadata including band names, number of rows and columns, data type, and compression information (either global or per band).
A MiraMon dataset can include multiple bands, all linked through a single `.rel` file.
Whether the name of one of the dataset's `.img` files or the `.rel` file is provided, the result will be the same: all bands will be taken into account.

The following are the main characteristics of a MiraMon raster dataset band:

- **IMG file**: Stores the raw raster data. The data type may vary:

  - *Byte*: 1 byte, unsigned. Range: 0 to 255
  - *Short*: 2 bytes, signed. Range: -32,768 to 32,767
  - *UShort*: 2 bytes, unsigned. Range: 0 to 65,535
  - *Int32*: 4 bytes, signed. Range: -2,147,483,648 to 2,147,483,647
  - *UInt32*: 4 bytes, unsigned. Range: 0 to 4,294,967,295
  - *Int64*: 8 bytes, signed integer
  - *UInt64*: 8 bytes, unsigned integer
  - *Real*: 4 bytes, floating-point
  - *Double*: 8 bytes, double precision floating-point

Encoding
--------

When reading MiraMon DBF files, the code page setting in the `.dbf` header is used to decode string fields to UTF-8,
regardless of whether the original encoding is ANSI or OEM.

REL files are always encoded in ANSI.

Open options
------------

None.

Dataset creation options
------------------------

None.

See Also
--------

-  `MiraMon's raster format specifications <https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf>`__
-  `MiraMon Extended DBF format <https://www.miramon.cat/new_note/eng/notes/DBF_estesa.pdf>`__
-  `MiraMon vector layer concepts <https://www.miramon.cat/help/eng/mm32/ap1.htm>`__.
-  `MiraMon page <https://www.miramon.cat/Index_usa.htm>`__
-  `MiraMon help guide <https://www.miramon.cat/help/eng>`__
-  `Grumets research group, the people behind MiraMon <https://www.grumets.cat/index_eng.htm>`__
