#include <osmium/handler.hpp>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>

#include "SpatiaLiteWriter.hpp"
#include "AreaCheck.hpp"

namespace bg	= boost::geometry;
namespace bgi	= bg::index;

class AreaIndex : public osmium::handler::Handler{
	typedef bg::model::point<double, 2, bg::cs::cartesian>	index_point_t;
	typedef bg::model::box<index_point_t>			index_box_t;
	typedef std::pair<index_box_t, Area*>			index_value_t;

	std::vector<index_value_t>	pending;
	bgi::rtree<index_value_t, bgi::rstar<256, 64>>	rtree;

	int64_t		id=0;

	osmium::geom::OGRFactory<>	m_factory;
	OGRSpatialReference		oSRS;
private:
	index_box_t region(Area *area);
public:
	AreaIndex();
	void findoverlapping(Area *area, std::vector<Area*> *list, AreaWant& want);
	void insert(Area *area);
	void create_rtree();
	void area(const osmium::Area& area);
	unsigned int foreach(AreaProcess& compare);
	unsigned int processoverlap(AreaCompare& compare);
};
