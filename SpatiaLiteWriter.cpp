// For assembling multipolygons

#include <osmium/area/assembler.hpp>
#include <osmium/geom/ogr.hpp>
#include <gdalcpp.hpp>

#include "Area.hpp"
#include "SpatiaLiteWriter.hpp"

#define DEBUG	0

void SpatiaLiteWriter::addAreaOverlapLayer(const char *name) {
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

	layer->add_field("style", OFTString, 20);

	layermap[name]=layer;
}

void SpatiaLiteWriter::addAreaLayer(const char *name) {
	gdalcpp::Layer *layer=new gdalcpp::Layer(dataset, name, wkbMultiPolygon);

	layer->add_field("area_id", OFTString, 20);
	layer->add_field("area_type", OFTString, 20);
	layer->add_field("area_changeset", OFTString, 20);
	layer->add_field("area_user", OFTString, 20);
	layer->add_field("area_timestamp", OFTString, 20);
	layer->add_field("area_key", OFTString, 20);
	layer->add_field("area_value", OFTString, 20);

	layer->add_field("errormsg", OFTString, 20);

	layer->add_field("style", OFTString, 20);

	layermap[name]=layer;
}


SpatiaLiteWriter::SpatiaLiteWriter(std::string &dbname) :
		dataset("sqlite", dbname, gdalcpp::SRS{}, {"SPATIALITE=TRUE", "INIT_WITH_EPSG=no"}) {

	dataset.exec("PRAGMA synchronous = OFF");

	addAreaOverlapLayer("overlap");
	addAreaOverlapLayer("natural");
	addAreaOverlapLayer("building");
	addAreaOverlapLayer("hierarchy");

	addAreaLayer("huge");
	addAreaLayer("suspicious");
}

void SpatiaLiteWriter::writeMultiPolygontoLayer(gdalcpp::Layer *layer, Area *a, Area *b, std::unique_ptr<OGRGeometry> mpoly, const char *style) {
	try  {
		gdalcpp::Feature feature{*layer, std::move(mpoly)};

		feature.set_field("area1_id", static_cast<double>(a->osm_id));
		feature.set_field("area1_type", a->source_string());
		feature.set_field("area1_changeset", static_cast<double>(a->osm_changeset));
		feature.set_field("area1_timestamp", a->osm_timestamp.to_iso().c_str());
		feature.set_field("area1_user", a->osm_user.c_str());
		feature.set_field("area1_key", a->osm_key);
		feature.set_field("area1_value", a->osm_value);

		feature.set_field("area2_id", static_cast<double>(b->osm_id));
		feature.set_field("area2_type", b->source_string());
		feature.set_field("area2_changeset", static_cast<double>(b->osm_changeset));
		feature.set_field("area2_timestamp", b->osm_timestamp.to_iso().c_str());
		feature.set_field("area2_user", b->osm_user.c_str());
		feature.set_field("area2_key", b->osm_key);
		feature.set_field("area2_value", b->osm_value);

		feature.set_field("style", style);

		feature.add_to_layer();

		std::cout
				<< a->osm_key << " " << a->osm_value << " "
				<< a->source_string() << " " << a->osm_id << " overlaps "
				<< b->osm_key << " " << b->osm_value << " "
				<< b->source_string() << " " << b->osm_id << " "
				<< "changesets "
				<< a->osm_changeset << "," <<  b->osm_changeset << " "
				<< a->osm_timestamp.to_iso() << "," << b->osm_timestamp.to_iso() << " "
				<< a->osm_user << "," << b->osm_user
				<< std::endl;

	} catch (gdalcpp::gdal_error) {
		std::cerr << "gdal_error while creating feature " << std::endl;
	}
}

void SpatiaLiteWriter::writeGeometry(gdalcpp::Layer *layer, Area *a, Area *b, OGRGeometry *geom, const char *style) {
	switch(geom->getGeometryType()) {
		case(wkbMultiPolygon): {
			std::unique_ptr<OGRGeometry>	g{geom->clone()};
			writeMultiPolygontoLayer(layer, a, b, std::move(g), style);
			break;
		}
		case(wkbPolygon): {
			std::unique_ptr<OGRMultiPolygon> mpoly{new OGRMultiPolygon()};
			mpoly->addGeometry(geom);
			writeMultiPolygontoLayer(layer, a, b, std::move(mpoly), style);
			break;
		}
		case(wkbGeometryCollection): {
			OGRGeometryCollection	*collection=(OGRGeometryCollection *) geom;
			for(int i=0;i<collection->getNumGeometries();i++) {
				OGRGeometry *sub=collection->getGeometryRef(i);
				writeGeometry(layer, a, b, sub, style);
				break;
			}
		}
	}
}

void SpatiaLiteWriter::write_overlap(Area *a, Area *b, const char *layername) {
	if (!a || !b || a->geometry == nullptr || b->geometry == nullptr)
		return;

	std::unique_ptr<OGRGeometry> intersection{a->geometry->Intersection(b->geometry)};

	if (!intersection)
		return;

	if (DEBUG) {
		std::cout << "Intersecion WKT" << std::endl;
		intersection->dumpReadable(stdout, nullptr, nullptr);
	}

	gdalcpp::Layer		*layer=layermap[layername];

	if (!layer) {
		std::cerr << "Undefined references layer " << layername << std::endl;
		abort();
	}

	writeGeometry(layer, a, b, intersection.get(), layername);
}

void SpatiaLiteWriter::writeAreaLayer(const char *layername, Area *a, const char *style, const char *errormsg) {
	gdalcpp::Layer		*layer=layermap[layername];
	try  {
		std::unique_ptr<OGRGeometry>	geom{a->geometry->clone()};
		gdalcpp::Feature feature{*layer, std::move(geom)};

		feature.set_field("area_id", static_cast<double>(a->osm_id));
		feature.set_field("area_type", a->source_string());
		feature.set_field("area_changeset", static_cast<double>(a->osm_changeset));
		feature.set_field("area_timestamp", a->osm_timestamp.to_iso().c_str());
		feature.set_field("area_user", a->osm_user.c_str());
		feature.set_field("area_key", a->osm_key);
		feature.set_field("area_value", a->osm_value);
		feature.set_field("errormsg", errormsg);

		feature.set_field("style", style);

		feature.add_to_layer();

		std::cout
				<< a->osm_key << " " << a->osm_value << " "
				<< a->source_string() << " " << a->osm_id
				<< " error " << errormsg
				<< std::endl;

	} catch (gdalcpp::gdal_error) {
		std::cerr << "gdal_error while creating feature " << std::endl;
	}
}


