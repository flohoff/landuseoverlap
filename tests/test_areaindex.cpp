#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstdio>

#include "helpers.hpp"
#include "AreaIndex.hpp"

namespace {
	std::string dbpath(const char *n) { return tmpfile(n); }

	struct Fixture {
		std::string		db;
		ReportStream		report;
		std::unique_ptr<SpatiaLiteWriter> writer;
		AreaIndex		index;

		Fixture(const char *n) : db(dbpath(n)) {
			std::remove(db.c_str());
			writer=std::make_unique<SpatiaLiteWriter>(db, report);
		}
		~Fixture() { writer.reset(); std::remove(db.c_str()); }

		void add(const Ring &r, const Tags &t, osmium::object_id_type id) {
			TestArea ta{r, t, id};
			index.area(ta.area());
		}
	};

	struct Collect : AreaCompare {
		mutable std::vector<std::pair<osmium::object_id_type, osmium::object_id_type>> pairs;
		int wanta=AREA_LANDUSE;
		Collect(SpatiaLiteWriter &w) : AreaCompare(w) {}
		bool WantA(Area *a) const override { return a->osm_type == wanta; }
		bool WantB(Area *a) const override { return a->osm_type == AREA_LANDUSE; }
		void Overlaps(Area *a, Area *b) const override { pairs.emplace_back(a->osm_id, b->osm_id); }
	};

	struct Proc : AreaProcess {
		mutable std::vector<osmium::object_id_type> seen;
		Proc(SpatiaLiteWriter &w) : AreaProcess(w) {}
		bool WantA(Area *a) const override { return a->osm_layer == 0 && a->osm_type == AREA_LANDUSE; }
		bool WantB(Area *) const override { return true; }
		void Process(Area *a) const override { seen.push_back(a->osm_id); }
	};
}

TEST_CASE("AreaIndex processoverlap finds bbox neighbours", "[index]") {
	Fixture f{"idx1.sqlite"};
	f.add(square(9.00, 52.00, 0.02), {{"landuse", "a"}}, 2);	// id 1
	f.add(square(9.01, 52.01, 0.02), {{"landuse", "b"}}, 4);	// id 2, overlaps 1
	f.add(square(10.0, 53.00, 0.01), {{"landuse", "c"}}, 6);	// id 3, far
	f.add(square(9.00, 52.00, 0.02), {{"amenity", "d"}}, 8);	// id 4, unwanted B
	f.index.create_rtree();

	Collect c{*f.writer};
	CHECK(f.index.processoverlap(c) == 3);

	std::sort(c.pairs.begin(), c.pairs.end());
	std::vector<std::pair<osmium::object_id_type, osmium::object_id_type>> want{
		{1,1},{1,2},{2,1},{2,2},{3,3}};
	CHECK(c.pairs == want);
}

TEST_CASE("AreaIndex processoverlap WantA filter", "[index]") {
	Fixture f{"idx2.sqlite"};
	f.add(square(9.00, 52.00, 0.02), {{"landuse", "a"}}, 2);
	f.add(square(9.00, 52.00, 0.02), {{"amenity", "d"}}, 4);
	f.index.create_rtree();

	Collect c{*f.writer};
	c.wanta=AREA_AMENITY;
	CHECK(f.index.processoverlap(c) == 1);
	REQUIRE(c.pairs.size() == 1);
	CHECK(c.pairs[0] == std::make_pair<osmium::object_id_type, osmium::object_id_type>(2, 1));
}

TEST_CASE("AreaIndex foreach", "[index]") {
	Fixture f{"idx3.sqlite"};
	f.add(square(9.0, 52.0, 0.01), {{"landuse", "a"}}, 2);
	f.add(square(9.1, 52.0, 0.01), {{"landuse", "b"}, {"layer", "1"}}, 4);
	f.add(square(9.2, 52.0, 0.01), {{"landuse", "c"}}, 6);
	f.add(square(9.3, 52.0, 0.01), {{"natural", "d"}}, 8);
	f.index.create_rtree();

	Proc p{*f.writer};
	CHECK(f.index.foreach(p) == 2);
	std::sort(p.seen.begin(), p.seen.end());
	CHECK(p.seen == std::vector<osmium::object_id_type>{1, 3});
}

TEST_CASE("AreaIndex empty", "[index]") {
	Fixture f{"idx4.sqlite"};
	f.index.create_rtree();
	Collect c{*f.writer};
	CHECK(f.index.processoverlap(c) == 0);
	Proc p{*f.writer};
	CHECK(f.index.foreach(p) == 0);
}

TEST_CASE("AreaIndex findoverlapping", "[index]") {
	Fixture f{"idx5.sqlite"};
	f.add(square(9.00, 52.00, 0.02), {{"landuse", "a"}}, 2);
	f.add(square(9.01, 52.01, 0.02), {{"landuse", "b"}}, 4);
	f.add(square(10.0, 53.00, 0.01), {{"landuse", "c"}}, 6);
	f.index.create_rtree();

	TestArea probe{square(9.015, 52.015, 0.001), {{"landuse", "p"}}, 100};
	Area pa{probe.geometry(), SRC_WAY, probe.area()};
	Collect c{*f.writer};
	std::vector<Area*> list;
	f.index.findoverlapping(&pa, &list, c);
	CHECK(list.size() == 2);
}

TEST_CASE("AreaIndex area() handles unclosed ring gracefully", "[index]") {
	Fixture f{"idx6.sqlite"};
	TestArea ta{{{9, 52}, {9.01, 52}, {9.01, 52.01}}, {{"landuse", "x"}}, 2};
	// must not throw
	REQUIRE_NOTHROW(f.index.area(ta.area()));
}

TEST_CASE("AreaIndex area() source from relation", "[index]") {
	Fixture f{"idx7.sqlite"};
	f.add(square(9.0, 52.0, 0.01), {{"landuse", "a"}}, 3);	// odd id: relation

	f.index.create_rtree();
	struct P : AreaProcess {
		mutable std::string src;
		P(SpatiaLiteWriter &w) : AreaProcess(w) {}
		bool WantA(Area *) const override { return true; }
		bool WantB(Area *) const override { return true; }
		void Process(Area *a) const override { src=a->source_string(); }
	} p{*f.writer};
	f.index.foreach(p);
	CHECK(p.src == "relation");
}
