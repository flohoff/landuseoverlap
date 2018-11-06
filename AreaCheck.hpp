#ifndef AREACHECK_HPP
#define AREACHECK_HPP

#include "Area.hpp"

class SpatiaLiteWriter;

class AreaWant {
	public:
	virtual bool WantA(Area *a) const = 0;
	virtual bool WantB(Area *a) const = 0;
};

class AreaProcess : public AreaWant {
	public:
	virtual bool WantA(Area *a) const = 0;
	virtual bool WantB(Area *a) const = 0;
	virtual void Process(Area *a, SpatiaLiteWriter& writer) const = 0;
};

class AreaCompare : public AreaWant {
	public:
	virtual bool WantA(Area *a) const = 0;
	virtual bool WantB(Area *a) const = 0;
	virtual const char *Overlaps(Area *a, Area *b) const = 0;
};

#endif
