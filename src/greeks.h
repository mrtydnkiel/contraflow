#ifndef GREEKS_H
#define GREEKS_H

#include "piping.h"
#include "utilities.h"

namespace contra 
{

class Greeks
{
public:
	Greeks() {}
	Greeks(double _beta_1, double _beta_12, double _beta, double _gamma, double _delta) :
		beta_1(_beta_1), beta_12(_beta_12), beta(_beta), gamma(_gamma), delta(_delta)
	{
		/*LOG("beta_1:  " << beta_1);
		LOG("beta_12: " << beta_12);
		LOG("beta:    " << beta);
		LOG("gamma:   " << gamma);
		LOG("delta:   " << delta);
		*/
	}

	double get_beta_1() const { return beta_1; }
	double get_beta_12() const { return beta_12; }
	double get_beta() const { return beta; }
	double get_gamma() const { return gamma; }
	double get_delta() const { return delta; }

private:
	double beta_1;
	double beta_12;
	double beta;
	double gamma;
	double delta;
};

}
#endif
