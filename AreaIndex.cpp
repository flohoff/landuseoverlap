
#include <gdalcpp.hpp>
#include <osmium/handler.hpp>
#include <spatialindex/capi/sidx_api.h>
#include <SpatialIndex.h>
#include <osmium/geom/ogr.hpp>

#include "Area.hpp"
#include "AreaCheck.hpp"
#include "AreaIndex.hpp"
#include "SpatiaLiteWriter.hpp"

#define DEBUG	0

namespace si = SpatialIndex;

template <typename AT>
class query_visitor : public si::IVisitor {
	std::vector<AT*>	*list;

	public:

	query_visitor(std::vector<AT*> *l) : list(l), m_io_index(0), m_io_leaf(0), m_io_found(0) {}

	void visitNode(si::INode const& n) {
		n.isLeaf() ? ++m_io_leaf : ++m_io_index;
	}

	void visitData(si::IData const& d) {
		uint64_t	id=d.getIdentifier();
		list->push_back((AT *)id);
		++m_io_found;
	}

	void visitData(std::vector<si::IData const*>& v) {
		assert(!v.empty()); (void)v;
	}

	size_t m_io_index;
	size_t m_io_leaf;
	size_t m_io_found;
};

si::Region AreaIndex::region(Area *area) {
	OGREnvelope	env;
	area->envelope(env);

	coord_array_t const p1 = { env.MinX, env.MinY };
	coord_array_t const p2 = { env.MaxX, env.MaxY };

	si::Region region(
		si::Point(p1.data(), p1.size()),
		si::Point(p2.data(), p2.size()));

	return region;
}

AreaIndex::AreaIndex() {
	sm=si::StorageManager::createNewMemoryStorageManager();
	rtree=si::RTree::createNewRTree(*sm, fill_factor, index_capacity, leaf_capacity, dimension, si::RTree::RV_LINEAR, index_id);

	oSRS.importFromEPSG(4326);
}

void AreaIndex::findoverlapping(Area *area, std::vector<Area*> *list) {
	query_visitor<Area> qvisitor{list};
	rtree->intersectsWithQuery(region(area), qvisitor);
}

void AreaIndex::insert(Area *area) {
	if (DEBUG)
		std::cout << "Insert: " << area->id << std::endl;
	rtree->insertData(0, nullptr, region(area), (uint64_t) area);
}

// This callback is called by osmium::apply for each area in the data.
void AreaIndex::area(const osmium::Area& area) {
	try {
		uint8_t		src=area.from_way() ? SRC_WAY : SRC_RELATION;

		std::unique_ptr<OGRGeometry>	geom=m_factory.create_multipolygon(area);
		geom->assignSpatialReference(&oSRS);
		Area	*a=new Area{std::move(geom), src, area};

		insert(a);
		arealist.push_back(a);
	} catch (const osmium::geometry_error& e) {
		std::cerr << "GEOMETRY ERROR: " << e.what() << "\n";
	} catch (osmium::invalid_location) {
		std::cerr << "Invalid location way id " << area.orig_id() << std::endl;
	}
}

void AreaIndex::foreach(SpatiaLiteWriter& writer, AreaProcess& compare) {
	const char *layername;
	for(auto ma : arealist) {
		if (!compare.Want(ma))
			continue;

		layername=compare.Process(ma);

		if (!layername)
			continue;
	}
}

void AreaIndex::processoverlap(SpatiaLiteWriter& writer, AreaCompare& compare) {
	std::vector<Area*>	list;
	list.reserve(100);
	const char *layername;

	for(auto ma : arealist) {

		if (!compare.Want(ma))
			continue;

		if (DEBUG)
			std::cout << "Checking overlap for " << ma->osm_id << std::endl;

		findoverlapping(ma, &list);

		for(auto oa : list) {
			if (DEBUG)
				std::cout << "\tIndex returned " << oa->osm_id << std::endl;

			layername=compare.Overlaps(ma, oa);

			if (!layername)
				continue;

			if (DEBUG) {
				std::cout << "\t\tPolygon overlaps " << oa->osm_id << std::endl;
				ma->dump();
				oa->dump();
			}

			writer.write_overlap(ma, oa, layername);
		}

		list.clear();
	}
}
