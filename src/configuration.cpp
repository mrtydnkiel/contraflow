#include "configuration.h"
#include "utilities.h"
#include "fluid.h"
#include "greeks.h"

#define _USE_MATH_DEFINES
#include <math.h>

#define _SQ2 1.41421356237
#define _PI 3.14159265359
#define _2PI 6.283185307179586
#define _4PI 12.5663706144
#define _8PI 25.132741228718345 

#define MULTIPOLE

namespace contra
{

Configuration_U::Configuration_U(Piping* _piping) : Configuration(_piping)
{
	DEBUG("U");
	piping->A_0 = M_PI * piping->d_0_i * piping->d_0_i / 4; 
	piping->A_1 = piping->A_0; 

	DEBUG("A_0: " << piping->A_0);

}

void Configuration_U::set_flow(double L)
{
	piping->u_0 = piping->Q / piping->A_0; 
	piping->u_1 = piping->u_0;

	piping->Re_0 = piping->fluid.Reynolds(piping->u_0, piping->d_0_i);
	piping->Re_1 = piping->Re_0;

	piping->Nu_0 = piping->fluid.Nusselt_pipe(piping->Re_0, L, piping->d_0_i);
	piping->Nu_1 = piping->Nu_0;
	
	DEBUG("u:  " << piping->u_0);
	DEBUG("Re: " << piping->Re_0);
	DEBUG("Nu: " << piping->Nu_0);
}

Resistances Configuration_U::set_resistances(double D, double lambda_g)
{
	R_adv = 1 / (piping->Nu_0 * piping->fluid.get_lambda() * M_PI);
	R_con_a = log(piping->d_0_o / piping->d_0_i) /(piping->lambda_0 * _2PI);

#ifdef MULTIPOLE 
	const double R_p = R_adv + R_con_a;

	const double theta_1 = piping->w / D;
	const double theta_2 = D / piping->d_0_o;
	const double theta_3 = piping->d_0_o / (2* piping->w);
	const double sigma = 0.; 
	const double beta = _2PI * lambda_g * R_p;

	const double th1_2 = theta_1 * theta_1;
	const double th1_4 = th1_2 * th1_2;
	const double _1_th1_4 = 1- th1_4;

	const double th3_2 = theta_3 * theta_3;

	const double ln_0 = log(theta_2 / pow(1 - th1_2, sigma));
	const double ln_01 = log(1. / ( 2 * theta_1 * pow(1 + th1_2, sigma)));

	double R_0 = (beta + ln_0);
	double R_01 = ln_01;

	///// 1st order term
	const double b = (1 - beta) / (1 + beta);

	const double nom_plus = th3_2 * pow(_1_th1_4 - 4 * sigma * th1_4, 2.); 
	const double denom_plus =  _1_th1_4 * _1_th1_4 + th3_2 * b * (_1_th1_4 * _1_th1_4 + 16 * sigma * th1_4);

	const double nom_minus = th3_2 * pow(_1_th1_4 + 4 * sigma * th1_4, 2.); 
	const double denom_minus =  _1_th1_4 * _1_th1_4 + th3_2 * b * (_1_th1_4 * _1_th1_4 + 8 * sigma * th1_2 * _1_th1_4);

	const double B_plus = b * nom_plus / denom_plus; 
	const double B_minus = b * nom_minus / denom_minus; 

	R_0 -= (B_plus + B_minus)/2;
	R_01 -= (B_plus - B_minus)/2;
	/////
	
	R_0 /= _2PI * lambda_g;
	R_01 /= _2PI * lambda_g;

	R_0_Delta = R_0 + R_01;
	R_1_Delta = R_0_Delta;
	R_01_Delta = (R_0*R_0 - R_01*R_01) / R_01;
#else
	const double D2 = D * D;
	const double d2 = piping->d_0_o * piping->d_0_o;
	const double w2 = piping->w * piping->w;
	const double l = _2PI * lambda_g;

	const double x = log(sqrt(D2 + 2 * d2)/(2*piping->d_0_o)) / 
		log(D/(_SQ2*piping->d_0_o));
	const double R_g = (1.601 - 0.888 * piping->w / D) * acosh((D2 + d2 - w2)/(2 * D * piping->d_0_o)) / l;
	const double R_ar = acosh((2*w2 - d2)/d2) / l;

	R_con_b = x * R_g;

	R_gs = (1 - x) * R_g;
	R_fg = R_adv + R_con_a + R_con_b;

	const double A = 2 * R_gs;
	const double B = R_ar - 2 * x * R_g;
	R_gg = A * B / (A - B);

	const double u_a = (1/R_fg) + (1/R_gs) + (1/R_gg);
	R_0_Delta = R_fg + R_gs;
	R_1_Delta = R_0_Delta;
	const double C = u_a * R_fg * R_gg;
	R_01_Delta = (C * C - R_fg * R_fg) / R_gg;

	DEBUG("x:          " << x);
	DEBUG("R_ar:       " << R_ar);
	DEBUG("R_con_b:    " << R_con_b);
	DEBUG("R_gs:       " << R_gs);
	DEBUG("R_fg:       " << R_fg);
	DEBUG("R_gg:       " << R_gg);
#endif
	const double R_b = R_0_Delta / 2.;
	const double R_a = 2 * R_01_Delta * R_0_Delta / (2* R_0_Delta + R_01_Delta);
	//const double R_g = R_b - (R_adv + R_con_a) / 2;

	DEBUG("R_a:        " << R_a);
	DEBUG("R_b:        " << R_b);
	//DEBUG("R_g:        " << R_g);

	DEBUG("R_adv:      " << R_adv);
	DEBUG("R_con_a:    " << R_con_a);

	DEBUG("R_0_Delta:  " << R_0_Delta);
	DEBUG("R_1_Delta:  " << R_1_Delta);
	DEBUG("R_01_Delta: " << R_01_Delta);
	return {R_0_Delta, R_1_Delta};
}

Greeks Configuration_U::set_greeks(Piping* piping)
{
	double factor = piping->get_fluid().get_c_vol() * piping->u_0 * piping->A_0;
	double beta_1 = 1. / (factor * R_0_Delta);
	double beta_12 = 1. / (factor * R_01_Delta);
	double gamma = sqrt(beta_1 * (beta_1 + 2 * beta_12));

	return Greeks(beta_1, beta_12, 0., gamma, (beta_1 + beta_12) / gamma);
}

void Configuration_U::set_functions(double& f1, double& f2, double& f3, const double& gz, const Greeks& greeks)
{
	const double ch = cosh(gz);
	const double sh = sinh(gz);

	f1 = ch - sh * greeks.get_delta();
	f2 = sh * greeks.get_beta_12() / greeks.get_gamma(); 
	f3 = ch + sh * greeks.get_delta();
}

double Configuration_U::F4(const double &z, const double &a, const double &b, const Greeks& greeks)
{
	const double gamma = greeks.get_gamma();
	const double beta_1 = greeks.get_beta_1();
	const double beta_12 = greeks.get_beta_12();
	const double delta = greeks.get_delta();

        return  (sinh(gamma*(z-a)) - sinh(gamma*(z-b))) * beta_1 / gamma + \
				(cosh(gamma*(z-b)) - cosh(gamma*(z-a))) *
                                (beta_1 / gamma) * (delta + beta_12 / gamma);
}

double Configuration_U::F5(const double &z, const double &a, const double &b, const Greeks& greeks)
{

	const double gamma = greeks.get_gamma();
	const double beta_1 = greeks.get_beta_1();
	const double beta_12 = greeks.get_beta_12();
	const double delta = greeks.get_delta();

        return (sinh(gamma*(z-a)) - sinh(gamma*(z-b))) * beta_1 / gamma \
				- (cosh(gamma*(z-b)) - cosh(gamma*(z-a))) *
                                (beta_1 / gamma) * (delta + beta_12 / gamma);
}

///// 2U


Configuration_2U::Configuration_2U(Piping* _piping) : Configuration(_piping)  
{
	DEBUG("2U");
	piping->A_0 = M_PI * piping->d_0_i * piping->d_0_i/4;
	piping->A_1 = M_PI * piping->d_1_i * piping->d_1_i/4;

	DEBUG("A_0: " << piping->A_0);
	DEBUG("A_1: " << piping->A_1);
}

void Configuration_2U::set_flow(double L)
{
	piping->u_0 = piping->Q / (2*piping->A_0);
	piping->u_1 = piping->Q / (2*piping->A_1);

	piping->Re_0 = piping->fluid.Reynolds(piping->u_0, piping->d_0_i);
	piping->Re_1 = piping->fluid.Reynolds(piping->u_1, piping->d_1_i);

	piping->Nu_0 = piping->fluid.Nusselt_pipe(piping->Re_0, L, piping->d_0_i);
	piping->Nu_1 = piping->fluid.Nusselt_pipe(piping->Re_1, L, piping->d_1_i);

	DEBUG("u_0:  " << piping->u_0);
	DEBUG("Re_0: " << piping->Re_0);
	DEBUG("Nu_0: " << piping->Nu_0);
	DEBUG("u_1:  " << piping->u_1);
	DEBUG("Re_1: " << piping->Re_1);
	DEBUG("Nu_1: " << piping->Nu_1);
}


Resistances Configuration_2U::set_resistances(double D, double lambda_g)
{

	const double D2 = D * D;
	const double d2 = piping->d_0_o * piping->d_0_o;
	const double s = piping->w;// *  1.41421356;
	const double s2 = s * s;
	const double l = _2PI * lambda_g;

	R_adv = 1 / (piping->Nu_0 * piping->fluid.get_lambda() * M_PI);
	R_con_a = log(piping->d_0_o / piping->d_0_i) /(piping->lambda_0 * _2PI);

#ifdef MULTIPOLE
	const double D4 = D2 * D2;
	const double D8 = D4 * D4;
	const double s4 = s2 * s2;
	const double s8 = s4 * s4;
	//const double d4 = d2 * d2;
	//const double d8 = d4 * d4;

	double R_p = R_adv + R_con_a;

	const double lambda_s = lambda_g;
	const double sigma = (lambda_g - lambda_s) / (lambda_g + lambda_s);
	const double beta = _2PI * lambda_g * R_p;	

	const double p_pc = d2 / (4*s2);
	const double p_c = s2 / pow(D8 - s8, .25);
	const double p_b = D2 / pow(D8 - s8, .25);
	const double b1 = (1 - beta)/(1 + beta);

	const double p_c2 = p_c * p_c;
	const double p_b2 = p_b * p_b;
	const double p_c4 = p_c2 * p_c2;
	const double p_b4 = p_b2 * p_b2;
	
	double R_b = log(D4/(4 * piping->d_0_o * s2*s)) + sigma * log(D8/(D8 - s8));
	// 1st order term
	R_b -= b1 * p_pc * pow(3-8*sigma* p_c4, 2.) / (1 + b1 * p_pc * (5 + 64 * sigma * p_c4 * p_b4));
	R_b /= _8PI * lambda_g;
	R_b += R_p/4;

	double R_a = log(s/piping->d_0_o) + sigma * log((D4 + s4)/(D4 - s4));
	// 1st order term
	R_a -= b1 * p_pc * pow(1+8*sigma* p_c2*p_b2, 2.) / (1 - b1 * p_pc * (3 - 32 * sigma * (p_c2 * p_b4 * p_b2 + p_c4 * p_c2 * p_b2)));
	R_a /= M_PI * lambda_g;
	R_a += 2*R_p;

	R_fg = 2 * R_b;
	R_gs = R_fg;
	R_gg_1 = 8 * R_b * (R_a - 2*R_b) / (4*R_b - R_a);
	R_gg_2 = R_gg_1;
	 
	DEBUG("R_a:    " << R_a);
	DEBUG("R_b:    " << R_b);
#else
	const double x = log(sqrt(D2 + 4 * d2)/(2*_SQ2*piping->d_0_o)) /
		log(D/(1.41421356*piping->d_0_o)); 
		//log(D/(2*piping->d_0_o));
	const double R_g = (3.098 - (4.432*s/D) + (2.364*s2/D2)) * acosh((D2 + d2 - s2)/(2 * D * piping->d_0_o)) / l;

	const double R_ar_1 = acosh((s2 - d2)/d2) / l;
	const double R_ar_2 = acosh((2*s2 - d2)/d2) / l;

	R_con_b = x * R_g;

	R_gs = (1 - x) * R_g;
	R_fg = R_adv + R_con_a + R_con_b;

	R_gg_1 = 2 * R_gs * (R_ar_1 - 2 * x * R_g) /(2*R_gs - R_ar_1 + 2 * x * R_g);
	R_gg_2 = 2 * R_gs * (R_ar_2 - 2 * x * R_g) /(2*R_gs - R_ar_2 + 2 * x * R_g);

	DEBUG("x:          " << x);
	DEBUG("R_g:        " << R_g);
	DEBUG("R_ar_1:     " << R_ar_1);
	DEBUG("R_ar_2:     " << R_ar_2);
#endif
	const double v = R_gg_1 * R_gg_2 / (2*(R_gg_1 + R_gg_2));
	const double u_a = (2/R_fg) + (2/R_gs) + (1/v);

	R_0_Delta = (R_fg + R_gs)/2;
	R_1_Delta = R_0_Delta;

	R_01_Delta = (u_a * u_a * v - 1/v) * R_fg * R_fg / 4;


	DEBUG("R_adv:      " << R_adv);
	DEBUG("R_con_a:    " << R_con_a);
	DEBUG("R_con_b:    " << R_con_b);
	DEBUG("R_gs:       " << R_gs);
	DEBUG("R_fg:       " << R_fg);
	DEBUG("R_gg_1:     " << R_gg_1);
	DEBUG("R_gg_2:     " << R_gg_2);

	DEBUG("R_0_Delta:  " << R_0_Delta);
	DEBUG("R_1_Delta:  " << R_1_Delta);
	DEBUG("R_01_Delta: " << R_01_Delta);
	return {R_0_Delta, R_1_Delta};
}


Greeks Configuration_2U::set_greeks(Piping* piping)
{
	double factor = piping->get_fluid().get_c_vol() * piping->u_0 * piping->A_0 * 2;
	double beta_1 = 1. / (factor * R_0_Delta);
	double beta_12 = 1. / (factor * R_01_Delta);
	double gamma = sqrt(beta_1 * (beta_1 + 2 * beta_12));

	return Greeks(beta_1, beta_12, 0., gamma, (beta_1 + beta_12) / gamma);
}

void Configuration_2U::set_functions(double& f1, double& f2, double& f3, const double& gz, const Greeks& greeks)
{
	const double ch = cosh(gz);
	const double sh = sinh(gz);

	f1 = ch - sh * greeks.get_delta();
	f2 = sh * greeks.get_beta_12() / greeks.get_gamma(); 
	f3 = ch + sh * greeks.get_delta();
}

double Configuration_2U::F4(const double &z, const double &a, const double &b, const Greeks& greeks)
{
	const double gamma = greeks.get_gamma();
	const double beta_1 = greeks.get_beta_1();
	const double beta_12 = greeks.get_beta_12();
	const double delta = greeks.get_delta();

        return  (sinh(gamma*(z-a)) - sinh(gamma*(z-b))) * beta_1 / gamma + \
				(cosh(gamma*(z-b)) - cosh(gamma*(z-a))) *
                                (beta_1 / gamma) * (delta + beta_12 / gamma);
}

double Configuration_2U::F5(const double &z, const double &a, const double &b, const Greeks& greeks)
{

	const double gamma = greeks.get_gamma();
	const double beta_1 = greeks.get_beta_1();
	const double beta_12 = greeks.get_beta_12();
	const double delta = greeks.get_delta();

        return (sinh(gamma*(z-a)) - sinh(gamma*(z-b))) * beta_1 / gamma \
				- (cosh(gamma*(z-b)) - cosh(gamma*(z-a))) *
                                (beta_1 / gamma) * (delta + beta_12 / gamma);
}

///// CX

Configuration_CX::Configuration_CX(Piping* _piping) : Configuration(_piping)  
{
	DEBUG("CX");
	piping->A_0 = piping->d_0_i * piping->d_0_i * M_PI / 4; 
	piping->A_1 = (piping->d_1_i * piping->d_1_i - piping->d_0_o * piping->d_0_o) * M_PI / 4;  
	DEBUG("A_0: " << piping->A_0);
	DEBUG("A_1: " << piping->A_1);
}

Greeks Configuration_CX::set_greeks(Piping* piping)
{
	double factor = piping->get_fluid().get_c_vol() * piping->u_0 * piping->A_0;
	double beta_1 = 1. / (factor * R_0_Delta);
	double beta_12 = 1. / (factor * R_01_Delta);

	double beta = - beta_1 / 2;
	double gamma = sqrt(beta_1 * (beta_1 * .25 + beta_12));

	return Greeks(beta_1, beta_12, beta, gamma, (beta_1*.5 + beta_12) / gamma);
}

void Configuration_CX::set_functions(double& f1, double& f2, double& f3, const double& gz, const Greeks& greeks)
{
	const double ch = cosh(gz);
	const double sh = sinh(gz);
	const double ebz = exp(greeks.get_beta() * gz / greeks.get_gamma());

	f1 = ebz * (ch - sh * greeks.get_delta());
	f2 = ebz * (sh * greeks.get_beta_12() / greeks.get_gamma()); 
	f3 = ebz * (ch + sh * greeks.get_delta());
}

double Configuration_CX::F4(const double &z, const double &a, const double &b, const Greeks& greeks)
{
	const double gamma = greeks.get_gamma();
	const double beta_1 = greeks.get_beta_1();
	const double beta_12 = greeks.get_beta_12();
	const double beta = greeks.get_beta();
	const double delta = greeks.get_delta();

	double ex = (exp(beta*(z-b)) - exp(beta*(z-a))) * beta_1 / (gamma*gamma - beta*beta);
	double co = (cosh(gamma*(z-b)) - cosh(gamma*(z-a))) * (gamma * delta + beta); 
	double si = (sinh(gamma*(z-a)) - sinh(gamma*(z-b))) * (gamma + delta*beta);
	
	return (co + si) * ex;
}

double Configuration_CX::F5(const double &z, const double &a, const double &b, const Greeks& greeks)
{

	const double gamma = greeks.get_gamma();
	const double beta_1 = greeks.get_beta_1();
	const double beta_12 = greeks.get_beta_12();
	const double beta = greeks.get_beta();
	const double delta = greeks.get_delta();

	double ex = (exp(beta*(z-b)) - exp(beta*(z-a))) * beta_1 *beta_12 / (gamma*gamma - beta*beta);
	double si = (sinh(gamma*(z-b)) - sinh(gamma*(z-a))) * beta / gamma; 
	double co = - (cosh(gamma*(z-b)) - cosh(gamma*(z-a)));
	return ex*(si+co);
}

void Configuration_CX::set_flow(double L)
{
	piping->u_0 = piping->Q / piping->A_0; 
	piping->u_1 = piping->Q / piping->A_1; 

	piping->Re_0 = piping->fluid.Reynolds(piping->u_0, piping->d_0_i);
	piping->Re_1 = piping->fluid.Reynolds(piping->u_1, piping->d_1_i - piping->d_0_o);

	piping->Nu_0 = piping->fluid.Nusselt_pipe(piping->Re_0, L, piping->d_0_i);
	piping->Nu_1 = piping->fluid.Nusselt_ring(piping->Re_1, L, piping->d_0_o, piping->d_1_i);
	
	DEBUG("u_0:  " << piping->u_0);
	DEBUG("u_1:  " << piping->u_1);
	DEBUG("Re_0: " << piping->Re_0);
	DEBUG("Re_1: " << piping->Re_1);
	DEBUG("Nu_0: " << piping->Nu_0);
	DEBUG("Nu_1: " << piping->Nu_1);
	
}
Resistances Configuration_CX::set_resistances(double D, double lambda_g)
{
	const double d = piping->d_1_i - piping->d_0_o;
	const double lnDd = log(D/(piping->d_1_o));
	const double lapi = piping->fluid.get_lambda() * M_PI;

	const double x = log(sqrt(D*D + piping->d_1_o * piping->d_1_o)/(_SQ2 * piping->d_1_o)) / lnDd;
	const double R_g = lnDd / (_2PI * lambda_g);

	R_adv_0 = 1 / (piping->Nu_0 * lapi);
	R_adv_1 = 1 / (piping->Nu_1 * lapi) * d / piping->d_0_o;
	R_adv_2 = 1 / (piping->Nu_1 * lapi) * d / piping->d_1_i;

	R_con_0_a = log(piping->d_0_o / piping->d_0_i) /(piping->lambda_0 * _2PI);
	R_con_1_a = log(piping->d_1_o / piping->d_1_i) /(piping->lambda_1 * _2PI);
	R_con_b = x * R_g;

	R_ff = R_adv_0 + R_adv_1 + R_con_0_a;
	R_fg = R_adv_2 + R_con_1_a + R_con_b;
	R_gs = (1 - x) * R_g;

	R_0_Delta = R_fg + R_gs;
	R_1_Delta = 0.;
	R_01_Delta = R_ff;

	DEBUG("x:          " << x);
	DEBUG("R_g:        " << R_g);

	DEBUG("R_adv_0:      " << R_adv_0);
	DEBUG("R_adv_1:      " << R_adv_1);
	DEBUG("R_adv_2:      " << R_adv_2);
	DEBUG("R_con_0_a:    " << R_con_0_a);
	DEBUG("R_con_1_a:    " << R_con_1_a);
	DEBUG("R_con_b:      " << R_con_b);
	DEBUG("R_ff:         " << R_ff);
	DEBUG("R_fg:         " << R_fg);
	DEBUG("R_gs:         " << R_gs);
	DEBUG("R_0_Delta:  " << R_0_Delta);
	DEBUG("R_1_Delta:  " << R_1_Delta);
	DEBUG("R_01_Delta: " << R_01_Delta);
	
	return {R_0_Delta, R_1_Delta};
}

}
