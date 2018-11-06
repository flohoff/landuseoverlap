#ifndef AREACHECK_HPP
#define AREACHECK_HPP

#include "Area.hpp"

class AreaProcess {
	public:
	virtual bool Want(Area *a) const = 0;
	virtual const char *Process(Area *a) const = 0;
};

class AreaCompare {
	public:
	virtual bool Want(Area *a) const = 0;
	virtual const char *Overlaps(Area *a, Area *b) const = 0;
};

#endif
