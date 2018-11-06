#include <osmium/handler.hpp>
#include <SpatialIndex.h>
#include <osmium/geom/ogr.hpp>

#include "SpatiaLiteWriter.hpp"
#include "AreaCheck.hpp"

namespace si = SpatialIndex;

class AreaIndex : public osmium::handler::Handler{
	si::ISpatialIndex	*rtree;
	si::IStorageManager	*sm;

	si::id_type index_id;
	uint32_t const index_capacity = 100;
	uint32_t const leaf_capacity = 100;
	uint32_t const dimension = 2;
	double const fill_factor = 0.5;

	typedef std::array<double, 2> coord_array_t;
	int64_t		id=0;

	osmium::geom::OGRFactory<>	m_factory;
	OGRSpatialReference		oSRS;
public:
	std::vector<Area*>			arealist;
private:
	si::Region region(Area *area);
public:
	AreaIndex();
	void findoverlapping(Area *area, std::vector<Area*> *list);
	void insert(Area *area);
	void area(const osmium::Area& area);
	void foreach(SpatiaLiteWriter& writer, AreaProcess& compare);
	void processoverlap(SpatiaLiteWriter& writer, AreaCompare& compare);
};
