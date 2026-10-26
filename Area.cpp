
#include <iostream>
#include <gdalcpp.hpp>
#include <osmium/osm/area.hpp>

#include "Area.hpp"

static uint64_t	globalid=0;

/* Prepared geometry cache: comparisons are made one lhs area at prepared handle */
namespace {
	struct CurrentPrepared {
		Area				*cur=nullptr;
		OGRPreparedGeometryH		handle=nullptr;

		~CurrentPrepared() {
			if (handle)
				OGRDestroyPreparedGeometry(handle);
		}
	};

	thread_local CurrentPrepared	t_prepared;

	OGRPreparedGeometryH prepared_for(Area *area) {
		if (t_prepared.cur==area)
			return t_prepared.handle;

		if (t_prepared.handle)
			OGRDestroyPreparedGeometry(t_prepared.handle);

		t_prepared.cur=area;
		t_prepared.handle=nullptr;

		if (area->geometry && OGRHasPreparedGeometrySupport())
			t_prepared.handle=OGRCreatePreparedGeometry(
				reinterpret_cast<OGRGeometryH>(area->geometry));

		return t_prepared.handle;
	}
}

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
	} else if (taglist.has_key("man_made")) {
		osm_key="man_made";
		osm_type=AREA_MANMADE;
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
	/* Use prepared geometry if available from cache */
	OGRPreparedGeometryH	prepared=prepared_for(this);

	if (prepared)
		return OGRPreparedGeometryIntersects(
			prepared,
			reinterpret_cast<OGRGeometryH>(oa->geometry));

	return geometry->Intersects(oa->geometry);
}

bool Area::intersects(Area *oa) {
	return geometry->Overlaps(oa->geometry);
}

const char *Area::source_string(void ) {
	return (source == SRC_WAY) ? "way" : "relation";
}

void Area::dump(void ) {
	std::cout << " Dump of area id " << id << " from OSM id " << osm_id << " type " << (int) source << std::endl;
	geometry->dumpReadable(stdout, nullptr, nullptr);
}
