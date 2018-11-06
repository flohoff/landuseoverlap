#ifndef AREA_HPP
#define AREA_HPP

#include <gdalcpp.hpp>
#include <osmium/osm/area.hpp>

enum {
	AREA_UNKNOWN,
	AREA_NATURAL,
	AREA_LANDUSE,
	AREA_AMENITY,
	AREA_LEISURE,
	AREA_BUILDING
};

enum {
	SRC_RELATION,
	SRC_WAY
};

class Area {
	public:
	const OGRGeometry			*geometry;
	uint8_t					source;

	uint64_t				id;

	uint8_t					osm_type;
	int					osm_layer=0;
	osmium::object_id_type			osm_id;
	std::string				osm_user;
	osmium::object_id_type			osm_changeset;
	osmium::Timestamp			osm_timestamp;
	const char				*osm_key;
	const char				*osm_value;


	~Area();
	Area(std::unique_ptr<OGRGeometry> geom, uint8_t otype, const osmium::Area &area);
	void envelope(OGREnvelope& env);
	bool overlaps(Area *oa);
	bool intersects(Area *oa);
	const char *source_string(void);
	float area(void);
	void dump(void );
};

#endif
