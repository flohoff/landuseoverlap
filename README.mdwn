
`landuseoverlap` analyses Openstreetmap Data for overlapping areas of different
kinds. 

There will be different layers in the resulting sqlite file.

* complex - Lists landuses/naturals with a high inner angular 
* hierarchy - Partial overlap between landuse/natural and other areas e.g. amenity/building/leisure
* huge - Very large landuses (greater 200ha)
* natural - Overlap between landuse and natural
* overlap - Overlapping or containing landuse 
* suspicious - Very small landuses (less than 100m²)

See <https://osm.zz.de/dbview/?db=landuseoverlap-nrw&layer=hierarchy#51.58133,7.48233,14z> as an example

Building
========

Building is only tested on Debian/Buster x86_64 but Ubuntu 18.04 should work aswell:

	apt-get -fuy install build-essential cmake libboost-dev git libgdal-dev libbz2-dev libexpat1-dev \
		libsparsehash-dev libboost-program-options-dev libgeos++-dev libproj-dev libspatialindex-dev
    
	cd landuseoverlap
	git submodule update --init
	cmake .
	make

Running
=======

Fetch a pbf extract for example from http://download.geofabrik.de then run:

	./landuseoverlap -i mylittle.pbf -d output.sqlite 

Output on stdout will be one problem per line. The sqlite is to be used with
[spatialite-rest](https://github.com/flohoff/spatialite-rest).

Copyright
=========

Copyright (C) 2026 Florian Lohoff <f@zz.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://gnu.org>.

