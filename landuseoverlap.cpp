
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
#include <boost/format.hpp>

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
		AreaOverlapCompare(SpatiaLiteWriter& writer) : AreaCompare(writer) {
			writer.addAreaOverlapLayer("overlap");
			writer.addAreaOverlapLayer("natural");
		};

		virtual bool WantA(Area *a) const {
			if (a->osm_type == AREA_LANDUSE
				|| a->osm_type == AREA_NATURAL)
				return true;
			return false;
		}

		virtual bool WantB(Area *a) const {
			return WantA(a);
		}

		virtual void Overlaps(Area *a, Area *b) const {
			/*
			 * Overlapping ourselves or an id smaller than ours
			 * We only want to check a -> b not b -> a again as they
			 * will overlap too anyway.
			 */
			if ((a->id >= b->id))
				return;

			if (a->osm_type != AREA_LANDUSE
				&& a->osm_type != AREA_NATURAL)
				return;

			if (b->osm_type != AREA_LANDUSE
				&& b->osm_type != AREA_NATURAL)
				return;

			if (a->overlaps(b)) {
				if (a->osm_type == AREA_NATURAL || b->osm_type == AREA_NATURAL)
					writer.write_overlap(a, b, "natural");
				else
					writer.write_overlap(a, b, "overlap");
				return;
			}

			return;
		}
};

class AmenityIntersect : public AreaCompare {
	public:
		AmenityIntersect(SpatiaLiteWriter& writer) : AreaCompare(writer) {
			writer.addAreaOverlapLayer("hierarchy");
		};

		virtual bool WantA(Area *a) const {
			if (a->osm_type == AREA_NATURAL)
				return true;
			if (a->osm_type == AREA_LANDUSE)
				return true;
			if (a->osm_type == AREA_AMENITY)
				return true;
			if (a->osm_type == AREA_MANMADE) {

				/* Overlapping types - by default */
				if (strcasecmp(a->osm_value, "pier") == 0)
					return false;
				if (strcasecmp(a->osm_value, "bridge") == 0)
					return false;

				return true;
			}
			if (a->osm_type == AREA_LEISURE) {
				if (strcasecmp(a->osm_value, "nature_reserve") == 0)
					return false;
				return true;
			}
			if (a->osm_type == AREA_BUILDING) {
				return true;
			}
			return false;
		}

		bool WantB(Area *a) const {
			return WantA(a);
		}

		void Overlaps(Area *a, Area *b) const {
			/*
			 * Overlapping ourselves or an id smaller than ours
			 * We only want to check a -> b not b -> a again as they
			 * will overlap too anyway.
			 */
			if ((a->id >= b->id) &&
				(a->osm_type == b->osm_type))
				return;

			if (DEBUG)
				std::cout << "Overlaps " << std::endl
					<< "A Id: " << a->osm_id
					<< "A Type: " << a->osm_key
					<< "B Id: " << b->osm_id
					<< "B Type: " << b->osm_key
					<< std::endl;

			/* One of them needs to be an AMENITY */
			if (!((WantA(a) && WantB(b))
				|| (WantA(b) && WantB(a))))
				return;

			if (DEBUG)
				std::cout << "Checking for intersection" << std::endl;

			if (a->intersects(b)) {

				/* if a builing overlaps something - check layers */
				if (a->osm_type == AREA_BUILDING
					|| b->osm_type == AREA_BUILDING) {

					if (a->osm_layer != b->osm_layer) {
						return;
					}
				}

				writer.write_overlap(a, b, "hierarchy");
				return;
			}

			return;
		}
};

class LanduseSize : public AreaProcess {
	OGRSpatialReference	tSRS;

	public:
		LanduseSize(SpatiaLiteWriter& writer) : AreaProcess(writer) {
			tSRS.importFromEPSG(31467);

			writer.addAreaLayer("huge");
			writer.addAreaLayer("suspicious");
			writer.addAreaLayer("complex");
		}

		bool WantA(Area *a) const {
			if (a->osm_type == AREA_LANDUSE)
				return true;
			return false;
		}

		bool WantB(Area *a) const {
			return WantA(a);
		}

		float polygon_area(OGRGeometry *geom) const {
			switch(geom->getGeometryType()) {
				case(wkbPolygon): {
					return static_cast<const OGRPolygon*>(geom)->get_Area();
				}
				case(wkbMultiPolygon): {
					return static_cast<const OGRMultiPolygon*>(geom)->get_Area();
				}
				default: {
					break;
				};
			}
			return 0;
		}

		double distance(const OGRPoint& a, const OGRPoint& b) const {
			return a.Distance((OGRGeometry *)&b);
		}

		double polygon_complexity(const OGRGeometry *geom) const {
			double			complexity=0;

			switch(geom->getGeometryType()) {
				case(wkbLineString): {
					const OGRLineString	*ring=static_cast<const OGRLineString*>(geom);
					int numpoints=ring->getNumPoints();

					// Summe der innenwinkel im dreieck
					if (numpoints <= 3)
						return 180;
					// Summe der innenwinkel im rechteck
					if (numpoints == 4)
						return 360;

					if (DEBUG)
						std::cout << "Looping on points" << std::endl;

					for(int i=0;i<numpoints;i++) {
						OGRPoint	Pa,Pb,Pc;

						ring->getPoint(i%(numpoints-1), &Pa);
						ring->getPoint((i+1)%(numpoints-1), &Pb);
						ring->getPoint((i+2)%(numpoints-1), &Pc);

						double a=distance(Pa, Pb);
						double b=distance(Pb, Pc);
						double c=distance(Pc, Pa);

						double rad=acos((a*a+b*b-c*c)/(2*a*b));
						double angle=rad*(180/3.1415926);

						if (DEBUG) {
							std::cout
								<< " Pa.X " << Pa.getX()
								<< " Pa.Y " << Pa.getY()
								<< " a " << a
								<< " b " << c
								<< " c " << a
								<< " rad " << rad
								<< " angle " << angle
								<< std::endl;
						}

						complexity+=(180-angle);
					}
					break;
				}
				case(wkbPolygon): {
					const OGRLinearRing	*lr=static_cast<const OGRPolygon*>(geom)->getExteriorRing();
					complexity+=polygon_complexity(lr);
					break;
				}
				case(wkbMultiPolygon): {
					const OGRMultiPolygon *mp=static_cast<const OGRMultiPolygon*>(geom);
					int numgeom=mp->getNumGeometries();

					for(int i=0;i<numgeom;i++) {
						const OGRGeometry *subgeom=mp->getGeometryRef(i);
						complexity+=polygon_complexity(subgeom);
					}
					break;
				}
				default: {
					std::cout << "Unknown geometry type " << geom->getGeometryName()
						<< "(" << geom->getGeometryType() << ")" << std::endl;
				};
			}

			return complexity;
		}

		void Process(Area *a) const {
			OGRGeometry	*geom=a->geometry->clone();
			geom->transformTo((OGRSpatialReference *) &tSRS);

			double complexity=polygon_complexity(geom);
			if (complexity > 2000) {
				std::string s=boost::str(boost::format("Complexity %1$.1f") % complexity);
				writer.writeAreaLayer("complex", a, "complex", s.c_str());
			}

			float areasize=polygon_area(geom);

			if (areasize < 40) {
				std::string s=boost::str(boost::format("Small landuse  %1$.2fm² below 40m²") % areasize);
				writer.writeAreaLayer("suspicious", a, "lsize1", s.c_str());
			} else if (areasize < 100) {
				std::string s=boost::str(boost::format("Small landuse  %1$.2fm² below 100m²") % areasize);
				writer.writeAreaLayer("suspicious", a, "lsize2", s.c_str());
			} else if (areasize > 400000) {
				std::string s=boost::str(boost::format("Huge landuse %1$.0fm² > 400000m²") % areasize);
				writer.writeAreaLayer("huge", a, "huge2", s.c_str());
			} else if (areasize > 200000) {
				std::string s=boost::str(boost::format("Large landuse %1$.0fm² > 200000m²") % areasize);
				writer.writeAreaLayer("huge", a, "huge1", s.c_str());
			}

			delete(geom);
		}
};


namespace po = boost::program_options;

int main(int argc, char* argv[]) {

	po::options_description         desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("infile,i", po::value<std::string>()->required(), "Input file")
		("dbname,d", po::value<std::string>()->required(), "Output database name")
	;

        po::variables_map vm;
        try {
                po::store(po::parse_command_line(argc, argv, desc), vm);
                po::notify(vm);
        } catch(const boost::program_options::error& e) {
                std::cerr << "Error: " << e.what() << std::endl;
		std::cerr << desc << std::endl;
                exit(-1);
        }

	AreaIndex	areahandler;

	osmium::io::File input_file{vm["infile"].as<std::string>()};

	osmium::area::Assembler::config_type assembler_config;

	osmium::TagsFilter areafilter{false};
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"landuse"}});
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"natural"}});
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"building"}});
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"amenity"}});
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"leisure"}});
	areafilter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"man_made"}});
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

	LanduseSize		ls{writer};
	areahandler.foreach(ls);

	AmenityIntersect	ai{writer};
	areahandler.processoverlap(ai);

	AreaOverlapCompare	luo{writer};
	areahandler.processoverlap(luo);

}
