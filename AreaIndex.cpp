
#include <osmium/handler.hpp>
#include <osmium/geom/ogr.hpp>

#include "Area.hpp"
#include "AreaCheck.hpp"
#include "AreaIndex.hpp"
#include "SpatiaLiteWriter.hpp"

#define DEBUG	0

AreaIndex::index_box_t AreaIndex::region(Area *area) {
	OGREnvelope	env;
	area->envelope(env);

	return index_box_t(
		index_point_t(env.MinX, env.MinY),
		index_point_t(env.MaxX, env.MaxY));
}

AreaIndex::AreaIndex() {
	oSRS.importFromEPSG(4326);
}

void AreaIndex::findoverlapping(Area *area, std::vector<Area*> *list, AreaWant& want) {
	std::vector<index_value_t>	hits;
	rtree.query(bgi::intersects(region(area)), std::back_inserter(hits));

	for(auto& v : hits) {
		if (want.WantB(v.second))
			list->push_back(v.second);
	}
}

void AreaIndex::insert(Area *area) {
	pending.emplace_back(region(area), area);
}

void AreaIndex::create_rtree() {
	bgi::rtree<index_value_t, bgi::rstar<256, 64>> loaded{pending.begin(), pending.end()};
	rtree=std::move(loaded);

	pending.clear();
	pending.shrink_to_fit();
}

void AreaIndex::area(const osmium::Area& area) {
	try {
		uint8_t		src=area.from_way() ? SRC_WAY : SRC_RELATION;

		std::unique_ptr<OGRGeometry>	geom=m_factory.create_multipolygon(area);
		geom->assignSpatialReference(&oSRS);
		Area	*a=new Area{std::move(geom), src, area};

		insert(a);
	} catch (const osmium::geometry_error& e) {
		std::cerr << "GEOMETRY ERROR: " << e.what() << "\n";
	} catch (const osmium::invalid_location& e) {
		std::cerr << "Invalid location way id " << area.orig_id() << std::endl;
	}
}

unsigned int AreaIndex::foreach(AreaProcess& compare) {
	unsigned int	numprocessed=0;

	for(auto it=rtree.begin(); it!=rtree.end(); ++it) {
		Area	*ma=it->second;

		if (!compare.WantA(ma))
			continue;

		numprocessed++;
		compare.Process(ma);
	}

	return numprocessed;
}

unsigned int AreaIndex::processoverlap(AreaCompare& compare) {
	std::vector<Area*>	list;
	list.reserve(100);
	unsigned int	numprocessed=0;

	for(auto it=rtree.begin(); it!=rtree.end(); ++it) {
		Area	*ma=it->second;

		if (!compare.WantA(ma))
			continue;

		numprocessed++;

		if (DEBUG)
			std::cout << "Checking overlap for " << ma->osm_id << std::endl;

		findoverlapping(ma, &list, compare);

		for(auto oa : list) {
			if (DEBUG)
				std::cout << "\tIndex returned " << oa->osm_id << std::endl;

			compare.Overlaps(ma, oa);
		}

		list.clear();
	}

	return numprocessed;
}
