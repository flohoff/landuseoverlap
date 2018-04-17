/*

  EXAMPLE osmium_area_test

  Create multipolygons from OSM data and dump them to stdout in one of two
  formats: WKT or using the built-in Dump format.

  DEMONSTRATES USE OF:
  * file input
  * location indexes and the NodeLocationsForWays handler
  * the MultipolygonManager and Assembler to assemble areas (multipolygons)
  * your own handler that works with areas (multipolygons)
  * the WKTFactory to write geometries in WKT format
  * the Dump handler
  * the DynamicHandler

  SIMPLER EXAMPLES you might want to understand first:
  * osmium_read
  * osmium_count
  * osmium_debug
  * osmium_amenity_list

  LICENSE
  The code in this example file is released into the Public Domain.

*/

#include <cstdlib>  // for std::exit
#include <cstring>  // for std::strcmp
#include <iostream> // for std::cout, std::cerr

// For assembling multipolygons
#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_manager.hpp>

// For the DynamicHandler class
#include <osmium/dynamic_handler.hpp>

// For the WKT factory
#include <osmium/geom/wkt.hpp>
#include <osmium/geom/ogr.hpp>

// For the Dump handler
#include <osmium/handler/dump.hpp>

// For the NodeLocationForWays handler
#include <osmium/handler/node_locations_for_ways.hpp>

// Allow any format of input files (XML, PBF, ...)
#include <osmium/io/any_input.hpp>

// For osmium::apply()
#include <osmium/visitor.hpp>

// For the location index. There are different types of indexes available.
// This will work for all input files keeping the index in memory.
#include <osmium/index/map/flex_mem.hpp>

#include <gdalcpp.hpp>
#include <boost/program_options.hpp>

#include <spatialindex/capi/sidx_api.h>
#include <SpatialIndex.h>
namespace si = SpatialIndex;

// The type of index used. This must match the include file above
using index_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;

// The location handler always depends on the index type
using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

#define DEBUG 0

uint64_t	globalid=0;

enum {
	SRC_RELATION,
	SRC_WAY
};

enum {
	AREA_UNKNOWN,
	AREA_NATURAL,
	AREA_LANDUSE
};

class myArea {
	public:
	std::unique_ptr<const OGRGeometry>	geometry;
	uint64_t				areaid;

	uint8_t					osmtype;
	uint8_t					areatype;
	const char				*key;
	const char				*value;
	osmium::object_id_type			osmid;
	osmium::object_id_type			changesetid;
	osmium::Timestamp			timestamp;
	std::string				user;

	myArea(std::unique_ptr<OGRGeometry> geom, uint8_t otype, const osmium::Area &area) :
			geometry(std::move(geom)), osmtype(otype),
			osmid(area.orig_id()), changesetid(area.changeset()),
			user(area.user()), timestamp(area.timestamp()) {

		const osmium::TagList& taglist=area.tags();
		if (taglist.has_key("natural")) {
			key="natural";
			areatype=AREA_NATURAL;
		} else if (taglist.has_key("landuse")) {
			key="landuse";
			areatype=AREA_LANDUSE;
		} else {
			key="unknown";
			areatype=AREA_UNKNOWN;
		}
		value=strdup(taglist.get_value_by_key(key, nullptr));

		areaid=globalid++;
	}

	uint64_t id(void ) {
		return areaid;
	}

	void envelope(OGREnvelope& env) {
		geometry->getEnvelope(&env);
	}

	bool overlaps(myArea *oa) {
		return geometry->Overlaps(oa->geometry.get())
			|| geometry->Contains(oa->geometry.get())
			|| geometry->Within(oa->geometry.get());
	}

	const char *type(void ) {
		return (osmtype == SRC_WAY) ? "way" : "relation";
	}

	void dump(void ) {
		std::cout << " Dump of area id " << areaid << " from OSM id " << osmid << " type " << (int) osmtype << std::endl;
		geometry->dumpReadable(stdout, nullptr, nullptr);
	}
};

class SpatiaLiteWriter : public osmium::handler::Handler {
    gdalcpp::Layer		*m_layer_overlap;
    gdalcpp::Layer		*m_layer_natural;
    gdalcpp::Dataset		dataset;
    osmium::geom::OGRFactory<>	m_factory{};
public:
    explicit SpatiaLiteWriter(std::string &dbname) :
	dataset("sqlite", dbname, gdalcpp::SRS{}, {  "SPATIALITE=TRUE", "INIT_WITH_EPSG=no" }) {

		dataset.exec("PRAGMA synchronous = OFF");

		m_layer_overlap=addAreaOverlapLayer("overlap");
		m_layer_natural=addAreaOverlapLayer("natural");
	}

	gdalcpp::Layer *addAreaOverlapLayer(const char *name) {
		gdalcpp::Layer *layer=new gdalcpp::Layer(dataset, name, wkbMultiPolygon);

		layer->add_field("area1_id", OFTString, 20);
		layer->add_field("area1_type", OFTString, 20);
		layer->add_field("area1_changeset", OFTString, 20);
		layer->add_field("area1_user", OFTString, 20);
		layer->add_field("area1_timestamp", OFTString, 20);
		layer->add_field("area1_key", OFTString, 20);
		layer->add_field("area1_value", OFTString, 20);

		layer->add_field("area2_id", OFTString, 20);
		layer->add_field("area2_type", OFTString, 20);
		layer->add_field("area2_changeset", OFTString, 20);
		layer->add_field("area2_user", OFTString, 20);
		layer->add_field("area2_timestamp", OFTString, 20);
		layer->add_field("area2_key", OFTString, 20);
		layer->add_field("area2_value", OFTString, 20);

		return layer;
	}

	void writeMultiPolygontoLayer(gdalcpp::Layer *layer, myArea *a, myArea *b, std::unique_ptr<OGRGeometry> mpoly) {
		try  {
			gdalcpp::Feature feature{*layer, std::move(mpoly)};

			feature.set_field("area1_id", static_cast<double>(a->osmid));
			feature.set_field("area1_type", a->type());
			feature.set_field("area1_changeset", static_cast<double>(a->changesetid));
			feature.set_field("area1_timestamp", a->timestamp.to_iso().c_str());
			feature.set_field("area1_user", a->user.c_str());
			feature.set_field("area1_key", b->key);
			feature.set_field("area1_value", b->key);

			feature.set_field("area2_id", static_cast<double>(b->osmid));
			feature.set_field("area2_type", b->type());
			feature.set_field("area2_changeset", static_cast<double>(b->changesetid));
			feature.set_field("area2_timestamp", b->timestamp.to_iso().c_str());
			feature.set_field("area2_user", b->user.c_str());
			feature.set_field("area2_key", b->key);
			feature.set_field("area2_value", b->key);

			feature.add_to_layer();


			std::cout
					<< a->key << " " << a->value << " "
					<< a->type() << " " << a->osmid << " overlaps "
					<< b->key << " " << b->value << " "
					<< b->type() << " " << b->osmid << " "
					<< "changesets "
					<< a->changesetid << "," <<  b->changesetid << " "
					<< a->timestamp.to_iso() << "," << b->timestamp.to_iso() << " "
					<< a->user << "," << b->user
					<< std::endl;

		} catch (gdalcpp::gdal_error) {
			std::cerr << "gdal_error while creating feature " << std::endl;
		}
	}

	void writeGeometry(gdalcpp::Layer *layer, myArea *a, myArea *b, OGRGeometry *geom) {
		switch(geom->getGeometryType()) {
			case(wkbMultiPolygon): {
				std::unique_ptr<OGRGeometry>	g{geom->clone()};
				writeMultiPolygontoLayer(layer, a, b, std::move(g));
				break;
			}
			case(wkbPolygon): {
				std::unique_ptr<OGRMultiPolygon> mpoly{new OGRMultiPolygon()};
				mpoly->addGeometry(geom);
				writeMultiPolygontoLayer(layer, a, b, std::move(mpoly));
				break;
			}
			case(wkbGeometryCollection): {
				OGRGeometryCollection	*collection=(OGRGeometryCollection *) geom;
				for(int i=0;i<collection->getNumGeometries();i++) {
					OGRGeometry *sub=collection->getGeometryRef(i);
					writeGeometry(layer, a, b, sub);
					break;
				}
			}
		}
	}

	void write_overlap(myArea *a, myArea *b) {
		if (!a || !b || a->geometry == nullptr || b->geometry == nullptr)
			return;

		std::unique_ptr<OGRGeometry> intersection{a->geometry->Intersection(b->geometry.get())};

		if (!intersection)
			return;

		if (DEBUG) {
			std::cout << "Intersecion WKT" << std::endl;
			intersection->dumpReadable(stdout, nullptr, nullptr);
		}

		gdalcpp::Layer		*layer=m_layer_overlap;
		if (a->areatype == AREA_NATURAL || b->areatype == AREA_NATURAL) {
			layer=m_layer_natural;
		}

		writeGeometry(layer, a, b, intersection.get());
	}
};


template <typename AT>
class query_visitor : public si::IVisitor {
   std::vector<AT*>	*list;
   public:
   query_visitor(std::vector<AT*> *l) : list(l), m_io_index(0), m_io_leaf(0), m_io_found(0) {}

    void visitNode(si::INode const& n)
    {
        n.isLeaf() ? ++m_io_leaf : ++m_io_index;
    }

    void visitData(si::IData const& d)
    {
        //si::IShape* ps = nullptr;
        //d.getShape(&ps);
        //std::unique_ptr<si::IShape> shape(ps);
        //; // use shape

        // Region is represented as array of characters
        uint8_t* pd = 0;
        uint32_t size = 0;
        d.getData(size, &pd);

	AT	*ptr;
	memcpy(&ptr, pd, sizeof(AT *));

	list->push_back(ptr);
	delete[] pd;

        //std::unique_ptr<uint8_t[]> data(pd);
        // use data
        //std::string str(reinterpret_cast<char*>(pd));

	//std::cout << "Overlap " << d.getIdentifier() << std::endl; // ID is query answer
        ++m_io_found;
    }

    void visitData(std::vector<si::IData const*>& v)
    {
        assert(!v.empty()); (void)v;
    }

    size_t m_io_index;
    size_t m_io_leaf;
    size_t m_io_found;
};

template <typename T>
class AreaIndex {
	si::ISpatialIndex	*rtree;
	si::IStorageManager	*sm;

	si::id_type index_id;
	uint32_t const index_capacity = 100;
	uint32_t const leaf_capacity = 100;
	uint32_t const dimension = 2;
	double const fill_factor = 0.5;

	typedef std::array<double, 2> coord_array_t;
	int64_t		id=0;
private:
	si::Region region(T *area) {
		OGREnvelope	env;
		area->envelope(env);

		coord_array_t const p1 = { env.MinX, env.MinY };
		coord_array_t const p2 = { env.MaxX, env.MaxY };

		si::Region region(
			si::Point(p1.data(), p1.size()),
			si::Point(p2.data(), p2.size()));

		return region;
	}
public:
	AreaIndex() {
		sm=si::StorageManager::createNewMemoryStorageManager();
		rtree=si::RTree::createNewRTree(*sm, fill_factor, index_capacity, leaf_capacity, dimension, si::RTree::RV_LINEAR, index_id);
	}

	void findoverlapping(T *area, std::vector<T*> *list) {
		query_visitor<T> qvisitor{list};
		rtree->intersectsWithQuery(region(area), qvisitor);
	}

	void insert(T *area) {
		if (DEBUG)
			std::cout << "Insert: " << area->id() << std::endl;
		rtree->insertData(sizeof(&area), (const byte *) &area, region(area), area->id());
	}
};

std::vector<myArea*>	arealist;
AreaIndex<myArea>	areaindex;

class OGRGen : public osmium::handler::Handler {
	osmium::geom::OGRFactory<>	m_factory;
public:
	// This callback is called by osmium::apply for each area in the data.
	void area(const osmium::Area& area) {
		try {
			uint8_t		src=area.from_way() ? SRC_WAY : SRC_RELATION;
			myArea		*a=new myArea{m_factory.create_multipolygon(area), src, area};

			areaindex.insert(a);
			arealist.push_back(a);
		} catch (const osmium::geometry_error& e) {
			std::cerr << "GEOMETRY ERROR: " << e.what() << "\n";
		} catch (osmium::invalid_location) {
			std::cerr << "Invalid location way id " << area.orig_id() << std::endl;
		}
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

    // Initialize an empty DynamicHandler. Later it will be associated
    // with one of the handlers. You can think of the DynamicHandler as
    // a kind of "variant handler" or a "pointer handler" pointing to the
    // real handler.
    osmium::handler::DynamicHandler handler;
    handler.set<OGRGen>();

    osmium::io::File input_file{vm["infile"].as<std::string>()};

    // Configuration for the multipolygon assembler. Here the default settings
    // are used, but you could change multiple settings.
    osmium::area::Assembler::config_type assembler_config;

    // Set up a filter matching only forests. This will be used to only build
    // areas with matching tags.
    osmium::TagsFilter filter{false};
    filter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"landuse"}});
    filter.add_rule(true, osmium::TagMatcher{osmium::StringMatcher::equal{"natural"}});

    // Initialize the MultipolygonManager. Its job is to collect all
    // relations and member ways needed for each area. It then calls an
    // instance of the osmium::area::Assembler class (with the given config)
    // to actually assemble one area. The filter parameter is optional, if
    // it is not set, all areas will be built.
    osmium::area::MultipolygonManager<osmium::area::Assembler> mp_manager{assembler_config, filter};

    // We read the input file twice. In the first pass, only relations are
    // read and fed into the multipolygon manager.
    std::cerr << "Pass 1...\n";
    osmium::relations::read_relations(input_file, mp_manager);
    std::cerr << "Pass 1 done\n";

    // Output the amount of main memory used so far. All multipolygon relations
    // are in memory now.
    std::cerr << "Memory:\n";
    osmium::relations::print_used_memory(std::cerr, mp_manager.used_memory());

    // The index storing all node locations.
    index_type index;

    // The handler that stores all node locations in the index and adds them
    // to the ways.
    location_handler_type location_handler{index};

    // If a location is not available in the index, we ignore it. It might
    // not be needed (if it is not part of a multipolygon relation), so why
    // create an error?
    location_handler.ignore_errors();

    // On the second pass we read all objects and run them first through the
    // node location handler and then the multipolygon collector. The collector
    // will put the areas it has created into the "buffer" which are then
    // fed through our "handler".
    std::cerr << "Pass 2...\n";
    osmium::io::Reader reader{input_file};
    osmium::apply(reader, location_handler, handler,
	mp_manager.handler([&handler](osmium::memory::Buffer&& buffer) {
        osmium::apply(buffer, handler);
    }));
    reader.close();
    std::cerr << "Pass 2 done\n";

    // Output the amount of main memory used so far. All complete multipolygon
    // relations have been cleaned up.
    std::cerr << "Memory:\n";
    osmium::relations::print_used_memory(std::cerr, mp_manager.used_memory());

    // If there were multipolgyon relations in the input, but some of their
    // members are not in the input file (which often happens for extracts)
    // this will write the IDs of the incomplete relations to stderr.
    std::vector<osmium::object_id_type> incomplete_relations_ids;
    mp_manager.for_each_incomplete_relation([&](const osmium::relations::RelationHandle& handle){
        incomplete_relations_ids.push_back(handle->id());
    });
    if (!incomplete_relations_ids.empty()) {
        std::cerr << "Warning! Some member ways missing for these multipolygon relations:";
        for (const auto id : incomplete_relations_ids) {
            std::cerr << " " << id;
        }
        std::cerr << "\n";
    }

	OGRRegisterAll();
	std::string		dbname=vm["dbname"].as<std::string>();
	SpatiaLiteWriter	writer{dbname};

	for(auto ma : arealist) {
		std::vector<myArea*>	list;
		if (DEBUG)
			std::cout << "Checking overlap for " << ma->osmid << std::endl;
		areaindex.findoverlapping(ma, &list);

		for(auto oa : list) {
			if (DEBUG)
				std::cout << "\tIndex returned " << oa->osmid << std::endl;

			/*
			 * Overlapping ourselves or an id smaller than ours
			 * We only want to check a -> b not b -> a again as they
			 * will overlap too anyway.
			 */
			if (oa->areaid <= ma->areaid)
				continue;

			if (DEBUG)
				std::cout << "\t\tChecking against " << oa->osmid << " areaids " << oa->areaid << "/" << ma->areaid << std::endl;

			if (oa->overlaps(ma)) {
				if (DEBUG) {
					std::cout << "\t\tPolygon overlaps " << oa->osmid << std::endl;
					ma->dump();
					oa->dump();
				}

				writer.write_overlap(ma, oa);
			}
		}
	}
}

