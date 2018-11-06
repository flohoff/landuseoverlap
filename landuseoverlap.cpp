
#include <cstdlib>  // for std::exit
#include <cstring>  // for std::strcmp
#include <iostream> // for std::cout, std::cerr

// For assembling multipolygons
#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_manager.hpp>

// For the DynamicHandler class
#include <osmium/dynamic_handler.hpp>

// For the Dump handler
#include <osmium/handler/dump.hpp>

// For the NodeLocationForWays handler
#include <osmium/handler/node_locations_for_ways.hpp>

// Allow any format of input files (XML, PBF, ...)
#include <osmium/io/any_input.hpp>

// For the location index. There are different types of indexes available.
// This will work for all input files keeping the index in memory.
#include <osmium/index/map/flex_mem.hpp>

#include <boost/program_options.hpp>

#include "SpatiaLiteWriter.hpp"
#include "Area.hpp"
#include "AreaIndex.hpp"
#include "AreaCheck.hpp"

// The type of index used. This must match the include file above
using index_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;

// The location handler always depends on the index type
using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

#define DEBUG 0

class AreaOverlapCompare : public AreaCompare {
	public:
		virtual bool Want(Area *a) const {
			if (a->osm_type == AREA_LANDUSE
				|| a->osm_type == AREA_NATURAL)
				return true;
			return false;
		}

		virtual const char *Overlaps(Area *a, Area *b) const {
			/*
			 * Overlapping ourselves or an id smaller than ours
			 * We only want to check a -> b not b -> a again as they
			 * will overlap too anyway.
			 */
			if ((a->id >= b->id))
				return nullptr;

			if (a->osm_type != AREA_LANDUSE
				&& a->osm_type != AREA_NATURAL)
				return nullptr;

			if (b->osm_type != AREA_LANDUSE
				&& b->osm_type != AREA_NATURAL)
				return nullptr;

			if (a->overlaps(b)) {
				if (a->osm_type == AREA_NATURAL || b->osm_type == AREA_NATURAL)
					return "natural";
				return "overlap";
			}

			return nullptr;
		}
};

class BuildingOverlap : public AreaOverlapCompare {
	public:
		virtual bool Want(Area *a) {
			if (a->osm_type == AREA_BUILDING)
				return true;
			return false;
		}

		virtual const char *Overlaps(Area *a, Area *b) {
			/*
			 * Overlapping ourselves or an id smaller than ours
			 * We only want to check a -> b not b -> a again as they
			 * will overlap too anyway.
			 */
			if (a->id >= b->id)
				return nullptr;

			if (a->osm_type != AREA_BUILDING ||
				b->osm_type != AREA_BUILDING)
				return nullptr;

			/* OSM Layer - If a roof is layer=1 dont overlap */
			if (a->osm_layer != b->osm_layer)
				return nullptr;

			if (a->overlaps(b))
				return "building";

			return nullptr;
		}
};

class AmenityIntersect : public AreaOverlapCompare {
	public:
		virtual bool Want(Area *a) const {
			if (a->osm_type == AREA_AMENITY)
				return true;
			if (a->osm_type == AREA_LEISURE) {
				if (strcasecmp(a->osm_value, "nature_reserve") == 0)
					return false;
				return true;
			}
			return false;
		}

		bool Matchagainst(Area *a) const {
			if (a->osm_type == AREA_NATURAL)
				return true;
			if (a->osm_type == AREA_LANDUSE)
				return true;

			return Want(a);
		}

		const char *Overlaps(Area *a, Area *b) const {
			/*
			 * Overlapping ourselves or an id smaller than ours
			 * We only want to check a -> b not b -> a again as they
			 * will overlap too anyway.
			 */
			if ((a->id >= b->id) &&
				(a->osm_type == b->osm_type))
				return nullptr;

			if (DEBUG)
				std::cout << "Overlaps " << std::endl
					<< "A Id: " << a->osm_id
					<< "A Type: " << a->osm_key
					<< "B Id: " << b->osm_id
					<< "B Type: " << b->osm_key
					<< std::endl;

			/* One of them needs to be an AMENITY */
			if (!((Want(a) && Matchagainst(b))
				|| (Want(b) && Matchagainst(a))))
				return nullptr;

			if (DEBUG)
				std::cout << "Checking for intersection" << std::endl;

			if (a->intersects(b))
				return "hierarchy";

			return nullptr;
		}
};

class LanduseSize : public AreaProcess {
	public:
		bool Want(Area *a) const {
			if (a->osm_type == AREA_LANDUSE)
				return true;
			return false;
		}

		const char *Process(Area *a) const {

			float as=a->area();

			if (as < 40) {
				std::cout << "Way id " << a->osm_id << " Area size " << as << std::endl;
			}

			if (as > 200000) {
				std::cout << "Way id " << a->osm_id << " Area size " << as << std::endl;
			}

			return nullptr;
		}
};


namespace po = boost::program_options;

int main(int argc, char* argv[]) {

	po::options_description         desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("infile,i", po::value<std::string>(), "Input file")
		("dbname,d", po::value<std::string>(), "Output database name")
	;
	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	AreaIndex	areahandler;

	osmium::io::File input_file{vm["infile"].as<std::string>()};

	osmium::area::Assembler::config_type assembler_config;

	osmium::TagsFilter areafilter{false};
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"landuse"}});
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"natural"}});
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"building"}});
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"amenity"}});
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"leisure"}});
	osmium::area::MultipolygonManager<osmium::area::Assembler> areamp_manager{assembler_config, areafilter};

	// We read the input file twice. In the first pass, only relations are
	// read and fed into the multipolygon manager.
	osmium::relations::read_relations(input_file, areamp_manager);

	index_type index;
	location_handler_type location_handler{index};
	location_handler.ignore_errors();

	osmium::io::Reader reader{input_file};
	osmium::apply(reader, location_handler,
		areahandler,
		areamp_manager.handler([&areahandler](osmium::memory::Buffer&& buffer) {
			osmium::apply(buffer, areahandler);
		})
	);
	reader.close();
	std::cerr << "Pass 2 done\n";

	std::cerr << "Memory:\n";
	osmium::relations::print_used_memory(std::cerr, areamp_manager.used_memory());

	OGRRegisterAll();
	std::string		dbname=vm["dbname"].as<std::string>();
	SpatiaLiteWriter	writer{dbname};

	LanduseSize		ls;
	areahandler.foreach(writer, ls);

	AmenityIntersect	ai;
	areahandler.processoverlap(writer, ai);

	AreaOverlapCompare	luo;
	areahandler.processoverlap(writer, luo);

	BuildingOverlap		bo;
	areahandler.processoverlap(writer, bo);

}
