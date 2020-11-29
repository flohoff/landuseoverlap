
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
	AreaWant&		want;

	public:

	query_visitor(std::vector<AT*> *l, AreaWant& want) : list(l), want(want){}

	void visitNode(si::INode const& n) {
	}

	void visitData(si::IData const& d) {
		uint64_t	id=d.getIdentifier();
		if (want.WantB((AT *)id))
			list->push_back((AT *)id);
	}

	void visitData(std::vector<si::IData const*>& v) {
		assert(!v.empty()); (void)v;
	}
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

void AreaIndex::findoverlapping(Area *area, std::vector<Area*> *list, AreaWant& want) {
	query_visitor<Area> qvisitor{list, want};
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
	for(auto ma : arealist) {
		if (!compare.WantA(ma))
			continue;

		compare.Process(ma, writer);
	}
}

void AreaIndex::processoverlap(SpatiaLiteWriter& writer, AreaCompare& compare) {
	std::vector<Area*>	list;
	list.reserve(100);
	const char *layername;

	for(auto ma : arealist) {

		if (!compare.WantA(ma))
			continue;

		if (DEBUG)
			std::cout << "Checking overlap for " << ma->osm_id << std::endl;

		findoverlapping(ma, &list, compare);

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
