#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <osmium/builder/attr.hpp>
#include <osmium/builder/osm_object_builder.hpp>
#include <osmium/geom/ogr.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/osm/area.hpp>

#include "Area.hpp"

inline std::string tmpfile(const char *n) {
	return (std::filesystem::temp_directory_path() / n).string();
}

using Ring = std::vector<std::pair<double, double>>;	// (lon, lat), closed
using Tags = std::vector<std::pair<std::string, std::string>>;

// Owns the buffer so the osmium::Area stays valid.
struct TestArea {
	osmium::memory::Buffer	buffer{4096, osmium::memory::Buffer::auto_grow::yes};

	TestArea(const Ring &ring, const Tags &tags, osmium::object_id_type id=2,
			const char *user="tester", osmium::changeset_id_type cs=7) {
		{
			osmium::builder::AreaBuilder ab{buffer};
			ab.set_id(id).set_version(1).set_changeset(cs)
				.set_timestamp(osmium::Timestamp{"2020-01-02T03:04:05Z"});
			ab.set_user(user);
			{
				osmium::builder::TagListBuilder tb{ab};
				for (auto &t : tags)
					tb.add_tag(t.first, t.second);
			}
			{
				osmium::builder::OuterRingBuilder rb{ab};
				osmium::object_id_type nid=1;
				for (auto &p : ring)
					rb.add_node_ref(osmium::NodeRef{nid++, osmium::Location{p.first, p.second}});
			}
		}
		buffer.commit();
	}

	const osmium::Area &area() { return buffer.get<osmium::Area>(0); }

	std::unique_ptr<OGRGeometry> geometry() {
		osmium::geom::OGRFactory<> f;
		return f.create_multipolygon(area());
	}
};

inline Ring square(double x, double y, double size) {
	return {{x, y}, {x+size, y}, {x+size, y+size}, {x, y+size}, {x, y}};
}

#endif
