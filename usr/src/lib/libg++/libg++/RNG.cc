#ifdef __GNUG__
#pragma implementation
#endif
#include <values.h>
#include <assert.h>
#include <builtin.h>
#include <RNG.h>

//
//	The scale constant is 2^-31. It is used to scale a 31 bit
//	long to a double.
//

//static const double randomDoubleScaleConstant = 4.656612873077392578125e-10;
//static const float  randomFloatScaleConstant = 4.656612873077392578125e-10;

static char initialized = 0;

RNG::RNG()
{
  if (!initialized)
  {

	assert (sizeof(double) == 2 * sizeof(unsigned long)); 

	//
	//	The following is a hack that I attribute to
	//	Andres Nowatzyk at CMU. The intent of the loop
	//	is to form the smallest number 0 <= x < 1.0,
	//	which is then used as a mask for two longwords.
	//	this gives us a fast way way to produce double
	//	precision numbers from longwords.
	//
	//	I know that this works for IEEE and VAX floating
	//	point representations.
	//
	//	A further complication is that gnu C will blow
	//	the following loop, unless compiled with -ffloat-store,
	//	because it uses extended representations for some of
	//	of the comparisons. Thus, we have the following hack.
	//	If we could specify #pragma optimize, we wouldn't need this.
	//

	PrivateRNGDoubleType t;
	PrivateRNGSingleType s;

#if _IEEE == 1
	
	t.d = 1.5;
	if ( t.u[1] == 0 ) {		// sun word order?
	    t.u[0] = 0x3fffffff;
	    t.u[1] = 0xffffffff;
	}
	else {
	    t.u[0] = 0xffffffff;	// encore word order?
	    t.u[1] = 0x3fffffff;
	}

	s.u = 0x3fffffff;
#else
	volatile double x = 1.0; // volatile needed when fp hardware used,
                             // and has greater precision than memory doubles
	double y = 0.5;
	do {			    // find largest fp-number < 2.0
	    t.d = x;
	    x += y;
	    y *= 0.5;
	} while (x != t.d && x < 2.0);

	volatile float xx = 1.0; // volatile needed when fp hardware used,
                             // and has greater precision than memory floats
	float yy = 0.5;
	do {			    // find largest fp-number < 2.0
	    s.s = xx;
	    xx += yy;
	    yy *= 0.5;
	} while (xx != s.s && xx < 2.0);
#endif
	// set doubleMantissa to 1 for each doubleMantissa bit
	doubleMantissa.d = 1.0;
	doubleMantissa.u[0] ^= t.u[0];
	doubleMantissa.u[1] ^= t.u[1];

	// set singleMantissa to 1 for each singleMantissa bit
	singleMantissa.s = 1.0;
	singleMantissa.u ^= s.u;

	initialized = 1;
    }
}
