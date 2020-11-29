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
	protected:
	SpatiaLiteWriter& writer;
	public:
	AreaProcess(SpatiaLiteWriter& writer) : writer(writer) {};
	virtual bool WantA(Area *a) const = 0;
	virtual bool WantB(Area *a) const = 0;
	virtual void Process(Area *a) const = 0;
};

class AreaCompare : public AreaWant {
	protected:
	SpatiaLiteWriter& writer;
	public:
	AreaCompare(SpatiaLiteWriter& writer) : writer(writer) {};
	virtual bool WantA(Area *a) const = 0;
	virtual bool WantB(Area *a) const = 0;
	virtual void Overlaps(Area *a, Area *b) const = 0;
};

#endif
