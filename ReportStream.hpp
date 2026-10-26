#ifndef REPORTSTREAM_HPP
#define REPORTSTREAM_HPP

#include <fstream>
#include <iostream>
#include <mutex>
#include <ostream>
#include <string>

/*
 * Report output stream for the worker threads. Holds the destination
 * stream (std::cout by default, an open file when -o is given) and the
 * mutex serializing the report lines right next to each other.
 *
 * Usage: report << ... << std::endl; The first << takes the mutex and
 * returns a proxy which keeps it until the statement ends (destructor
 * of the temporary), so a whole line can be written with plain <<
 * without assembling a string or locking by hand.
 */
class ReportStream {
	std::ofstream			file;
	std::ostream			*stream;
	bool				tofile{false};
	std::mutex			mutex;

	// Holds the mutex while one report line is being assembled.
	class Line {
		ReportStream		*rs;

		public:
		// Writes the first fragment, keeping the lock.
		template<typename T>
		Line(ReportStream *r, const T &value) : rs(r) {
			rs->mutex.lock();
			*rs->stream << value;
		}

		Line(const Line&) = delete;
		Line& operator=(const Line&) = delete;

		// Release the lock when the statement ends. No explicit
		// flush: line writers end with std::endl, which already
		// flushes, and any remaining buffered content is written
		// when the stream is closed.
		~Line() {
			rs->mutex.unlock();
		}

		template<typename T>
		Line& operator<<(const T &value) {
			*rs->stream << value;
			return *this;
		}

		// So std::endl, std::flush and friends work.
		Line& operator<<(std::ostream& (*manip)(std::ostream&)) {
			manip(*rs->stream);
			return *this;
		}
	};

	public:
	ReportStream() : stream(&std::cout) {}

	ReportStream(const std::string &filename) : stream(&std::cout), tofile(true) {
		file.open(filename, std::ios_base::out | std::ios_base::trunc);
		if (file.is_open())
			stream=&file;
	}

	// false if a file was requested but could not be opened
	bool valid() const { return !tofile || file.is_open(); }

	template<typename T>
	Line operator<<(const T &value) {
		return Line(this, value);
	}
};

#endif
