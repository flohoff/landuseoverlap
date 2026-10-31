#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <sstream>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_approx.hpp>
#include "helpers.hpp"

static Area *make(TestArea &ta) {
	return new Area{ta.geometry(), SRC_WAY, ta.area()};
}

TEST_CASE("Area classifies by tag key", "[area]") {
	struct { const char *key; int type; } cases[] = {
		{"natural", AREA_NATURAL}, {"landuse", AREA_LANDUSE},
		{"building", AREA_BUILDING}, {"amenity", AREA_AMENITY},
		{"leisure", AREA_LEISURE}, {"man_made", AREA_MANMADE},
	};
	for (auto &c : cases) {
		TestArea ta{square(9, 52, 0.01), {{c.key, "val"}}};
		std::unique_ptr<Area> a{make(ta)};
		CHECK(a->osm_type == c.type);
		CHECK(std::string(a->osm_key) == c.key);
		CHECK(std::string(a->osm_value) == "val");
	}
}

TEST_CASE("Area key priority", "[area]") {
	TestArea ta{square(9, 52, 0.01), {{"amenity", "x"}, {"natural", "wood"}, {"landuse", "y"}}};
	std::unique_ptr<Area> a{make(ta)};
	CHECK(a->osm_type == AREA_NATURAL);
	CHECK(std::string(a->osm_value) == "wood");

	TestArea tb{square(9, 52, 0.01), {{"building", "yes"}, {"amenity", "y"}}};
	std::unique_ptr<Area> b{make(tb)};
	CHECK(b->osm_type == AREA_BUILDING);

	TestArea tc{square(9, 52, 0.01), {{"leisure", "park"}, {"man_made", "pier"}}};
	std::unique_ptr<Area> c{make(tc)};
	CHECK(c->osm_type == AREA_LEISURE);

	TestArea td{square(9, 52, 0.01), {{"landuse", "a"}, {"building", "yes"}}};
	std::unique_ptr<Area> d{make(td)};
	CHECK(d->osm_type == AREA_LANDUSE);

	TestArea te{square(9, 52, 0.01), {{"amenity", "a"}, {"leisure", "b"}}};
	std::unique_ptr<Area> e{make(te)};
	CHECK(e->osm_type == AREA_AMENITY);
}

TEST_CASE("Area metadata and ids", "[area]") {
	TestArea ta{square(9, 52, 0.01), {{"landuse", "meadow"}}, 202, "alice", 99};
	std::unique_ptr<Area> a{make(ta)};
	CHECK(a->osm_id == 101);
	CHECK(a->osm_changeset == 99);
	CHECK(a->osm_user == "alice");
	CHECK(a->osm_timestamp.to_iso() == "2020-01-02T03:04:05Z");
	CHECK(a->osm_layer == 0);
	CHECK(a->source == SRC_WAY);

	TestArea tb{square(9, 52, 0.01), {{"landuse", "meadow"}}};
	std::unique_ptr<Area> b{make(tb)};
	CHECK(b->id == a->id + 1);
}

TEST_CASE("Area layer tag", "[area]") {
	TestArea ta{square(9, 52, 0.01), {{"landuse", "meadow"}, {"layer", "-2"}}};
	std::unique_ptr<Area> a{make(ta)};
	CHECK(a->osm_layer == -2);
}

TEST_CASE("Area source_string", "[area]") {
	TestArea ta{square(9, 52, 0.01), {{"landuse", "meadow"}}};
	Area w{ta.geometry(), SRC_WAY, ta.area()};
	Area r{ta.geometry(), SRC_RELATION, ta.area()};
	CHECK(std::string(w.source_string()) == "way");
	CHECK(std::string(r.source_string()) == "relation");
}

TEST_CASE("Area envelope", "[area]") {
	TestArea ta{square(9, 52, 0.5), {{"landuse", "meadow"}}};
	std::unique_ptr<Area> a{make(ta)};
	OGREnvelope env;
	a->envelope(env);
	CHECK(env.MinX == Catch::Approx(9.0));
	CHECK(env.MinY == Catch::Approx(52.0));
	CHECK(env.MaxX == Catch::Approx(9.5));
	CHECK(env.MaxY == Catch::Approx(52.5));
}

TEST_CASE("Area intersects", "[area]") {
	TestArea t1{square(9, 52, 0.02), {{"landuse", "a"}}};
	TestArea t2{square(9.01, 52.01, 0.02), {{"landuse", "b"}}};
	TestArea t3{square(10, 53, 0.01), {{"landuse", "c"}}};
	std::unique_ptr<Area> a1{make(t1)}, a2{make(t2)}, a3{make(t3)};

	CHECK(a1->intersects(a2.get()));
	CHECK(a2->intersects(a1.get()));	// switches prepared cache
	CHECK_FALSE(a1->intersects(a3.get()));
	CHECK(a1->intersects(a1.get()));
	CHECK_FALSE(a3->intersects(a2.get()));
}

TEST_CASE("Area overlaps", "[area]") {
	TestArea t1{square(9, 52, 0.02), {{"landuse", "a"}}};
	TestArea t2{square(9.01, 52.01, 0.02), {{"landuse", "b"}}};
	TestArea t3{square(9.005, 52.005, 0.005), {{"landuse", "c"}}};	// contained
	TestArea t4{square(10, 53, 0.01), {{"landuse", "d"}}};
	std::unique_ptr<Area> a1{make(t1)}, a2{make(t2)}, a3{make(t3)}, a4{make(t4)};

	CHECK(a1->overlaps(a2.get()));
	CHECK_FALSE(a1->overlaps(a3.get()));	// contains, not overlaps
	CHECK_FALSE(a1->overlaps(a4.get()));
}

TEST_CASE("Area dump prints id", "[area]") {
	TestArea ta{square(9, 52, 0.01), {{"landuse", "meadow"}}, 42};
	std::unique_ptr<Area> a{make(ta)};
	std::ostringstream out;
	auto *old=std::cout.rdbuf(out.rdbuf());
	a->dump();
	std::cout.rdbuf(old);
	CHECK(out.str().find("OSM id 21") != std::string::npos);
	CHECK(out.str().find("type 1") != std::string::npos);
}
