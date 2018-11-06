
#include <iostream>
#include <gdalcpp.hpp>
#include <osmium/osm/area.hpp>

#include "Area.hpp"

static uint64_t	globalid=0;

Area::~Area(void ) {
	delete(geometry);
}

Area::Area(std::unique_ptr<OGRGeometry> geom, uint8_t otype, const osmium::Area &area) :
		geometry(geom.release()), source(otype),
		osm_id(area.orig_id()), osm_changeset(area.changeset()),
		osm_user(area.user()), osm_timestamp(area.timestamp()) {

	const osmium::TagList& taglist=area.tags();
	if (taglist.has_key("natural")) {
		osm_key="natural";
		osm_type=AREA_NATURAL;
	} else if (taglist.has_key("landuse")) {
		osm_key="landuse";
		osm_type=AREA_LANDUSE;
	} else if (taglist.has_key("building")) {
		osm_key="building";
		osm_type=AREA_BUILDING;
	} else if (taglist.has_key("amenity")) {
		osm_key="amenity";
		osm_type=AREA_AMENITY;
	} else if (taglist.has_key("leisure")) {
		osm_key="leisure";
		osm_type=AREA_LEISURE;
	} else {
		osm_key="unknown";
		osm_type=AREA_UNKNOWN;
		// This case segfaults later
	}

	osm_value=strdup(taglist.get_value_by_key(osm_key, nullptr));

	if (taglist.has_key("layer")) {
		const char *layerstring=taglist.get_value_by_key("layer", nullptr);
		osm_layer=atoi(layerstring);
	}

	id=globalid++;
}

void Area::envelope(OGREnvelope& env) {
	geometry->getEnvelope(&env);
}

bool Area::overlaps(Area *oa) {
	return geometry->Overlaps(oa->geometry)
		|| geometry->Contains(oa->geometry)
		|| geometry->Within(oa->geometry);
}

bool Area::intersects(Area *oa) {
	return geometry->Overlaps(oa->geometry);
}

const char *Area::source_string(void ) {
	return (source == SRC_WAY) ? "way" : "relation";
}

float Area::area(void ) {
	OGRGeometry		*geom=geometry->clone();
	float			area=0;
	OGRSpatialReference	tSRS;

	tSRS.importFromEPSG(31467);
	geom->transformTo(&tSRS);

	switch(geom->getGeometryType()) {
		case(wkbPolygon): {
			area=static_cast<const OGRPolygon*>(geom)->get_Area();
			break;
		}
		case(wkbMultiPolygon): {
			area=static_cast<const OGRMultiPolygon*>(geom)->get_Area();
			break;
		}
		default: {
			area=0;
		};
	}

	delete(geom);
	return area;
}

void Area::dump(void ) {
	std::cout << " Dump of area id " << id << " from OSM id " << osm_id << " type " << (int) source << std::endl;
	geometry->dumpReadable(stdout, nullptr, nullptr);
}


