// [[Rcpp::depends(RcppArmadillo)]]
// [[Rcpp::plugins(cpp11)]]

#include "optim.hpp"

/*
 * Newton tests
 *
 * $CXX -O3 -Wall -std=c++11 -march=native -ffp-contract=fast -I$ARMA_INCLUDE_PATH -I$OPTIM/include logit_reg.cpp -o logit_reg.test -L$OPTIM -loptim -framework Accelerate
 */

// sigmoid function

inline
arma::mat sigm(const arma::mat& X)
{
    return 1.0 / (1.0 + arma::exp(-X));
}

// log-likelihood function data

struct ll_data_t
{
    arma::vec Y;
    arma::mat X;
};

// log-likelihood function with hessian

double ll_fn_whess(const arma::vec& vals_inp, arma::vec* grad_out, arma::mat* hess_out, void* opt_data)
{
    ll_data_t* objfn_data = reinterpret_cast<ll_data_t*>(opt_data);

    arma::vec Y = objfn_data->Y;
    arma::mat X = objfn_data->X;

    arma::vec mu = sigm(X*vals_inp);

    const double norm_term = static_cast<double>(Y.n_elem);

    const double obj_val = - arma::accu( Y%arma::log(mu) + (1.0-Y)%arma::log(1.0-mu) ) / norm_term;

    //

    if (grad_out)
    {
        *grad_out = X.t() * (mu - Y) / norm_term;
    }

    //

    if (hess_out)
    {
        arma::mat S = arma::diagmat( mu%(1.0-mu) );
        *hess_out = X.t() * S * X / norm_term;
    }

    //

    return obj_val;
}

// log-likelihood function for Adam

double ll_fn(const arma::vec& vals_inp, arma::vec* grad_out, void* opt_data)
{
    return ll_fn_whess(vals_inp,grad_out,nullptr,opt_data);
}

//' logit_optim
//'
//' A simple example optimizing logit regression with optimLib gradient descent
//'
//' @export
// [[Rcpp::export]]
Rcpp::NumericVector logit_optimLib() {
    int n_dim = 5;     // dimension of parameter vector
    int n_samp = 4000; // sample length

    arma::mat X = arma::randn(n_samp,n_dim);
    arma::vec theta_0 = 1.0 + 3.0*arma::randu(n_dim,1);

    arma::vec mu = sigm(X*theta_0);

    arma::vec Y(n_samp);

    for (int i=0; i < n_samp; i++)
    {
        Y(i) = ( arma::as_scalar(arma::randu(1)) < mu(i) ) ? 1.0 : 0.0;
    }

    // fn data and initial values

    ll_data_t opt_data;
    opt_data.Y = std::move(Y);
    opt_data.X = std::move(X);

    arma::vec x = arma::ones(n_dim,1) + 1.0; // initial values

    // run Adam-based optim

    optim::algo_settings_t settings;

    settings.gd_method = 6;
    settings.gd_settings.step_size = 0.1;

    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

    bool success = optim::gd(x,ll_fn,&opt_data,settings);

    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;

    Rcpp::Rcout << "\nAdam: true values vs estimates:\n" << arma::join_rows(theta_0,x) << std::endl;

    //
    // run Newton-based optim

    x = arma::ones(n_dim,1) + 1.0; // initial values

    start = std::chrono::system_clock::now();

    success = optim::newton(x,ll_fn_whess,&opt_data);

    end = std::chrono::system_clock::now();
    elapsed_seconds = end-start;

    Rcpp::Rcout << "\nnewton: true values vs estimates:\n" << arma::join_rows(theta_0,x) << std::endl;

    return Rcpp::wrap(x);
}
