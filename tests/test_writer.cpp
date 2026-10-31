#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <ogrsf_frmts.h>

#include "helpers.hpp"
#include "SpatiaLiteWriter.hpp"

namespace {
	long featurecount(const std::string &db, const char *layer, std::string *field=nullptr, const char *fname=nullptr) {
		GDALDataset *ds=(GDALDataset*) GDALOpenEx(db.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
		if (!ds) return -1;
		OGRLayer *l=ds->GetLayerByName(layer);
		long n=l ? l->GetFeatureCount() : -1;
		if (l && field && fname) {
			l->ResetReading();
			if (OGRFeature *ft=l->GetNextFeature()) {
				*field=ft->GetFieldAsString(fname);
				OGRFeature::DestroyFeature(ft);
			}
		}
		GDALClose(ds);
		return n;
	}
}

TEST_CASE("SpatiaLiteWriter write_overlap", "[writer]") {
	std::string db=tmpfile("wr1.sqlite");
	std::string rep=tmpfile("wr1.report");
	std::remove(db.c_str());

	TestArea t1{square(9, 52, 0.02), {{"landuse", "a"}}, 2, "u1", 11};
	TestArea t2{square(9.01, 52.01, 0.02), {{"natural", "b"}}, 4, "u2", 12};
	TestArea t3{square(10, 53, 0.01), {{"landuse", "c"}}, 6};
	Area a1{t1.geometry(), SRC_WAY, t1.area()};
	Area a2{t2.geometry(), SRC_RELATION, t2.area()};
	Area a3{t3.geometry(), SRC_WAY, t3.area()};

	{
		ReportStream report{rep};
		SpatiaLiteWriter w{db, report};
		w.addAreaOverlapLayer("overlap");
		w.addAreaLayer("huge");

		w.write_overlap(&a1, &a2, "overlap");
		w.write_overlap(&a1, nullptr, "overlap");	// ignored
		w.writeAreaLayer("huge", &a1, "huge1", "too big");
	}

	std::string v;
	CHECK(featurecount(db, "overlap", &v, "area1_user") == 1);
	CHECK(v == "u1");
	CHECK(featurecount(db, "overlap", &v, "area2_value") == 1);
	CHECK(v == "b");
	CHECK(featurecount(db, "overlap", &v, "style") == 1);
	CHECK(v == "overlap");
	CHECK(featurecount(db, "huge", &v, "errormsg") == 1);
	CHECK(v == "too big");

	std::ifstream in{rep};
	std::string l1, l2;
	std::getline(in, l1);
	std::getline(in, l2);
	CHECK(l1 == "landuse a way 1 overlaps natural b relation 2 changesets 11,12 "
		"2020-01-02T03:04:05Z,2020-01-02T03:04:05Z u1,u2");
	CHECK(l2 == "landuse a way 1 error too big");

	std::remove(db.c_str());
	std::remove(rep.c_str());
}

TEST_CASE("SpatiaLiteWriter write_overlap polygon intersection", "[writer]") {
	std::string db=tmpfile("wr2.sqlite");
	std::remove(db.c_str());
	TestArea t1{square(9, 52, 0.02), {{"landuse", "a"}}, 2};
	TestArea t2{square(9.01, 52.01, 0.02), {{"landuse", "b"}}, 4};
	Area a1{t1.geometry(), SRC_WAY, t1.area()};
	Area a2{t2.geometry(), SRC_WAY, t2.area()};
	{
		ReportStream report{"/dev/null"};
		SpatiaLiteWriter w{db, report};
		w.addAreaOverlapLayer("overlap");
		w.write_overlap(&a1, &a2, "overlap");
		w.write_overlap(&a2, &a1, "overlap");
	}
	CHECK(featurecount(db, "overlap") == 2);
	std::remove(db.c_str());
}

TEST_CASE("SpatiaLiteWriter write_overlap geometry collection", "[writer]") {
	std::string db=tmpfile("wr3.sqlite");
	std::remove(db.c_str());
	TestArea t1{square(9, 52, 0.02), {{"landuse", "a"}}, 2};

	auto wkt=[](const char *s) {
		OGRGeometry *g=nullptr;
		OGRGeometryFactory::createFromWkt(s, nullptr, &g);
		return std::unique_ptr<OGRGeometry>{g};
	};
	// intersection is polygon + line segment
	Area a1{wkt("MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)),((3 0,4 0,4 1,3 1,3 0)))"),
		SRC_WAY, t1.area()};
	Area a2{wkt("MULTIPOLYGON(((1 1,2.5 1,2.5 2,1 2,1 1)),((2.5 0.2,3 0.2,3 0.8,2.5 0.8,2.5 0.2)))"),
		SRC_WAY, t1.area()};
	{
		ReportStream report{"/dev/null"};
		SpatiaLiteWriter w{db, report};
		w.addAreaOverlapLayer("overlap");
		w.write_overlap(&a1, &a2, "overlap");
	}
	CHECK(featurecount(db, "overlap") == 1);
	std::remove(db.c_str());
}
