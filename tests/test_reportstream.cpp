#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#include "ReportStream.hpp"
#include "helpers.hpp"

TEST_CASE("ReportStream default is valid and uses stdout", "[report]") {
	ReportStream rs;
	CHECK(rs.valid());

	std::ostringstream out;
	auto *old=std::cout.rdbuf(out.rdbuf());
	rs << "a " << 1 << ' ' << 2.5 << std::endl;
	std::cout.rdbuf(old);
	CHECK(out.str() == "a 1 2.5\n");
}

TEST_CASE("ReportStream writes to file", "[report]") {
	std::string fn=tmpfile("report-test.tmp");
	{
		ReportStream rs{fn};
		CHECK(rs.valid());
		rs << "line " << 42 << std::endl;
		rs << "second" << std::endl;
	}
	std::ifstream in{fn};
	std::string l1, l2, l3;
	std::getline(in, l1);
	std::getline(in, l2);
	CHECK_FALSE(std::getline(in, l3));
	CHECK(l1 == "line 42");
	CHECK(l2 == "second");
	std::remove(fn.c_str());
}

TEST_CASE("ReportStream invalid file", "[report]") {
	ReportStream rs{"/nonexistent-dir/x/report"};
	CHECK_FALSE(rs.valid());
}

TEST_CASE("ReportStream lines do not interleave", "[report]") {
	std::string fn=tmpfile("report-mt.tmp");
	{
		ReportStream rs{fn};
		std::vector<std::thread> th;
		for (int t=0; t<4; t++)
			th.emplace_back([&rs, t] {
				for (int i=0; i<200; i++)
					rs << "T" << t << " " << "aaaa" << " " << "bbbb" << " end" << std::endl;
			});
		for (auto &x : th)
			x.join();
	}
	std::ifstream in{fn};
	std::string l;
	int n=0;
	while (std::getline(in, l)) {
		n++;
		CHECK(l.size() == 16);
		CHECK(l.substr(2) == " aaaa bbbb end");
	}
	CHECK(n == 800);
	std::remove(fn.c_str());
}
