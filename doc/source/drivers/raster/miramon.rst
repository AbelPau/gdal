.. _raster.miramon:

MiraMon Raster
==============

.. versionadded:: 3.12

.. shortname:: MiraMonRaster

.. built_in_by_default::

This driver is capable of translating (reading) raster files from MiraMon raster format.

A `look-up-table of MiraMon <https://www.miramon.cat/help/eng/mm32/AP6.htm>`__ and
`EPSG <https://epsg.org/home.html>`__ Spatial Reference Systems allows matching
identifiers in both systems.

If a layer contains an old *.rel* format file (used some decades ago),
a warning message will appear explaining how to convert it into a modern *.rel 4* file.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Overview of MiraMon format
--------------------------

The MiraMon .img format is a binary format for raster data with rich metadata.
More information about the MiraMon raster format is available `on the public
specification <https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf>`__.

By providing only the main file name, the driver will automatically use the sidecar metadata file. 
REL file can be used to convert all bands in it.

These are the main characteristics of MiraMon rasters dataset:

- **img file**: This is the main file containing only the data. The data can be of various types:

    - *Byte*: 1 byte numbers. Range: 0 to 255
    - *Short*: 2 bytes signed short integer numbers. Range: -32,768 to 32,767
    - *UShort*: 2 bytes unsigned short integer numbers. Range: 0 to 65,535
    - *Int32*: 4 bytes signed integer numbers. Range: -2,147,483,648 to 2,147,483,647
    - *UInt32*: 4 bytes unsigned integer numbers. Range: 0 to 4,294,967,295
    - *Int64*: 8 bytes signed integer numbers.
    - *UInt64*: 8 bytes unsigned integer numbers.
    - *Real*: 4 bytes real numbers.
    - *Double*: 8 bytes real numbers.

- **rel file**: This is the file containing metadata information such as number of rows, columns, data type, if it's compressed.

Encoding
--------

When reading MiraMon files the code page setting in the header of the .dbf file
is read and used to translate string fields to UTF-8 (regardless of whether they
are in ANSI, OEM or UTF-8).

Open options
------------

None

Dataset creation options
------------------------

None

See Also
--------

-  `MiraMon's raster format specifications <https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf>`__
-  `MiraMon Extended DBF format <https://www.miramon.cat/new_note/eng/notes/DBF_estesa.pdf>`__
-  `MiraMon vector layer concepts <https://www.miramon.cat/help/eng/mm32/ap1.htm>`__.
-  `MiraMon page <https://www.miramon.cat/Index_usa.htm>`__
-  `MiraMon help guide <https://www.miramon.cat/help/eng>`__
-  `Grumets research group, the people behind MiraMon <https://www.grumets.cat/index_eng.htm>`__
