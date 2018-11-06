#ifndef SPATIALITEWRITER_HPP
#define SPATIALITEWRITER_HPP

#include <osmium/handler.hpp>
#include <gdalcpp.hpp>
#include <osmium/geom/ogr.hpp>

#include "Area.hpp"

class SpatiaLiteWriter : public osmium::handler::Handler {
	gdalcpp::Dataset		dataset;
	osmium::geom::OGRFactory<>	m_factory{};

	std::map<std::string, gdalcpp::Layer*>	layermap;

	public:
	SpatiaLiteWriter(std::string &dbname);
	void write_overlap(Area *a, Area *b, const char *layername);

	private:
	void writeGeometry(gdalcpp::Layer *layer, Area *a, Area *b, OGRGeometry *geom, const char *style);
	void writeMultiPolygontoLayer(gdalcpp::Layer *layer, Area *a, Area *b, std::unique_ptr<OGRGeometry> mpoly, const char *style);
	void addAreaOverlapLayer(const char *name);
};

#endif
