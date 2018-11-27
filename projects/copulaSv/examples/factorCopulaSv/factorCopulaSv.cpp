#define ARMA_NO_DEBUG
#include <RcppArmadillo.h>
// #include <gperftools/profiler.h>

#include "main/algorithms/mcmc/pmmh.h"
#include "main/algorithms/smc/SmcSampler.h"
#include "main/applications/factorCopulaSv/factorCopulaSv.h"
#include "time.h"

// TODO: disable range checks (by using at() for indexing elements of cubes/matrices/vectors)
// once the code is tested; 
// To that end, compile the code with ARMA_NO_DEBUG defined.

// Enable C++11 via this plugin (Rcpp 0.10.3 or later)
// [[Rcpp::plugins("cpp11")]]

////////////////////////////////////////////////////////////////////////////////
// Samples from the prior of the parameters.
////////////////////////////////////////////////////////////////////////////////
// [[Rcpp::depends("RcppArmadillo")]]
// [[Rcpp::export]]
arma::colvec sampleFromPriorCpp
(
  const unsigned int dimTheta, // length of the parameter vector
  const arma::colvec& hyperParameters, // hyper parameters
  const arma::mat& support, // matrix containing the boundaries of the suppport of the parameters
  const unsigned int nCores = 1 // number of cores to use
)
{ 
  arma::colvec theta;
  sampleFromPrior<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations>(theta, dimTheta, hyperParameters, support, nCores);
  return theta;
}

////////////////////////////////////////////////////////////////////////////////
// Simulates observations.
//////////////////////////////////////////////////////////////////////////////
// [[Rcpp::depends("RcppArmadillo")]]
// [[Rcpp::export]]
Rcpp::List simulateDataCpp
(
  const unsigned int nObservations, // number of observations
  const arma::colvec& hyperParameters, // hyper parameters 
  const arma::colvec& theta, // parameters
  const unsigned int nCores = 1// number of cores to use
)
{
  LatentPath latentPath;
  Observations observations;
  
  arma::colvec extraParameters;
  
  std::mt19937 engine; 
  RngDerived<std::mt19937> rngDerived(engine);
  Model<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations> model(rngDerived, hyperParameters, nCores);
  model.setUnknownParameters(theta);
  model.setnObservations(nObservations);
  model.simulateData(extraParameters);

  return Rcpp::List::create
  (
    Rcpp::Named("latentTrue")   = model.getLatentPath(),
    Rcpp::Named("logExchangeRates")        = model.getObservations().logExchangeRates_, 
    Rcpp::Named("logVolatilities")         = model.getObservations().logVolatilities_,
    Rcpp::Named("initialLogExchangeRates") = model.getObservations().initialLogExchangeRates_, 
    Rcpp::Named("initialLogVolatilities")  = model.getObservations().initialLogVolatilities_,
    Rcpp::Named("alphaTrue")    = model.getModelParameters().getAlpha(),
    Rcpp::Named("betaTrue")     = model.getModelParameters().getBeta(),
    Rcpp::Named("kappaTrue")    = model.getModelParameters().getKappa(),
    Rcpp::Named("muTrue")       = model.getModelParameters().getMu(),
    Rcpp::Named("sigmaTrue")    = model.getModelParameters().getSigma(),
    Rcpp::Named("lambdaHTrue")  = model.getModelParameters().getLambdaH(),
    Rcpp::Named("lambdaZTrue")  = model.getModelParameters().getLambdaZ(),
    Rcpp::Named("lambdaHZTrue") = model.getModelParameters().getLambdaHZ(),
    Rcpp::Named("omega")        = model.getModelParameters().getOmega(),
    Rcpp::Named("copulaTypeH")  = static_cast<unsigned int>(model.getModelParameters().getCopulaTypeH()),
    Rcpp::Named("copulaTypeZ")  = static_cast<unsigned int>(model.getModelParameters().getCopulaTypeZ()),
    Rcpp::Named("copulaTypeHZ") = static_cast<unsigned int>(model.getModelParameters().getCopulaTypeHZ())
  );
}


////////////////////////////////////////////////////////////////////////////////
// Runs a PMMH algorithm 
////////////////////////////////////////////////////////////////////////////////
// [[Rcpp::depends("RcppArmadillo")]]
// [[Rcpp::export]]
Rcpp::List runPmmhCpp
(
  const arma::mat& logExchangeRates,         // (K, T)-matrix of log-exchange rates
  const arma::mat& logVolatilities,          // (K, T)-matrix of log-volatilities
  const arma::colvec& initialLogExchangeRates,  // length-K vector of log-exchange rates
  const arma::colvec& initialLogVolatilities,   // length-K vector of log-volatilities
  const unsigned int dimTheta,               // length of the parameter vector
  const arma::colvec& hyperParameters,       // hyperparameters and other auxiliary model parameters
  const arma::mat& support,                  // (par.size(), 2)-matrix containing the lower and upper bounds of the support of each parameter
  const unsigned int nIterations,            // number of MCMC iterations
  const unsigned int nParticles,             // number of particles per MCMC iteration within each lower-level SMC algorithm
  const double essResamplingThreshold,       // ESS-based resampling threshold for the lower-level SMC algorithms
  const arma::colvec& smcParameters,         // additional parameters to be passed to the particle filter
  const arma::colvec& mcmcParameters,        // additional parameters to be passed to the MCMC kernel
  const bool useAdaptiveProposal,            // should we adapt the proposal scale of the MCMC kernel as in Peters at al. (2010)?
  const bool useAdaptiveProposalScaleFactor1, // should we also adapt the constant by which the sample covariance matrix is multiplied?
  const arma::colvec& adaptiveProposalParameters, // parameters needed for the adaptive mixture proposal from Peters at al. (2010).
  const arma::colvec& rwmhSd,                // scaling of the random-walk Metropolis--Hastings proposals
  const arma::colvec& thetaInit,             // initial value for theta (if we keep theta fixed throughout) 
  const double burninPercentage,             // percentage iterations to be thrown away as burnin
  const bool samplePath,                     // store particle paths?
  const unsigned int nCores                  // number of nCores used (currently, this is not implemented)
)
{

  std::mt19937 engine; 
  RngDerived<std::mt19937> rngDerived(engine);
  
  std::cout << "setting up observations" << std::endl;
  
  unsigned int nObservations = logExchangeRates.n_cols; // number of observations
  unsigned int nSteps = nObservations; // number of lower-level SMC steps
  Observations observations; // observations 
  observations.logExchangeRates_ = logExchangeRates;
  observations.logVolatilities_  = logVolatilities;
  observations.initialLogExchangeRates_ = initialLogExchangeRates;
  observations.initialLogVolatilities_  = initialLogVolatilities;
     
  std::cout << "setting up model class" << std::endl;
  // Model class.
  Model<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations> model(rngDerived, hyperParameters, observations, nObservations, nCores);
  model.setSupport(support);
  model.setDimTheta(dimTheta);
  
  std::cout << "setting up SMC class" << std::endl;
  
  Smc<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations, Particle, Aux, SmcParameters> smc(
    rngDerived, model, nSteps,
    static_cast<SmcProposalType>(0), 
    essResamplingThreshold,
    static_cast<SmcBackwardSamplingType>(0),
    false,
    1,
    nCores
  );
  smc.setUseGaussianParametrisation(false);
  smc.setNParticles(nParticles);
  smc.setSamplePath(samplePath);
  smc.setNLookaheadSteps(smcParameters(0));
  
  std::cout << "setting up MCMC class" << std::endl;

  // Class for running MCMC algorithms.
  Mcmc<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations, McmcParameters> mcmc(
    rngDerived, model, nCores
  );
  mcmc.setRwmhSd(rwmhSd);
  mcmc.setUseAdaptiveProposal(useAdaptiveProposal);
  mcmc.setUseDelayedAcceptance(false);
  mcmc.setAdaptiveProposalParameters(adaptiveProposalParameters, nIterations);
  mcmc.setUseAdaptiveProposalScaleFactor1(useAdaptiveProposalScaleFactor1);
  mcmc.setNIterations(nIterations, burninPercentage);
  
  std::vector<arma::colvec> theta(nIterations); // parameters sampled by the algorithm
  
//   std::vector<arma::umat> latentPath(nIterations); // one latent path sampled and stored at each iteration
  std::vector<LatentPath> latentPaths(nIterations); // one latent path sampled and stored at each iteration
  double cpuTime; // total amount of time needed for running the algorithm
  double acceptanceRate; // (second-stage) acceptance rate after burn-in
  double acceptanceRateAux; // unused
  
  std::cout << "running PMMH" << std::endl;
  
  runPmmh<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations, Particle, Aux, SmcParameters,McmcParameters>
    (theta, latentPaths, cpuTime, acceptanceRateAux, acceptanceRate, rngDerived, model, smc, mcmc, thetaInit, samplePath, nCores);
  
  return Rcpp::List::create(
    Rcpp::Named("theta")          = theta, 
//     Rcpp::Named("latentPath")    = latentPath, 
    Rcpp::Named("cpuTime")        = cpuTime, // total time needed for running the algorithm
    Rcpp::Named("acceptanceRate") = acceptanceRate // acceptance rate after burn-in
  );
}

////////////////////////////////////////////////////////////////////////////////
// Runs an SMC sampler to estimate the model evidence
////////////////////////////////////////////////////////////////////////////////
// [[Rcpp::depends("RcppArmadillo")]]
// [[Rcpp::export]]
Rcpp::List runSmcSamplerCpp
(
  const arma::mat& logExchangeRates,         // (K, T)-matrix of log-exchange rates
  const arma::mat& logVolatilities,          // (K, T)-matrix of log-volatilities
  const arma::colvec& initialLogExchangeRates,  // length-K vector of log-exchange rates
  const arma::colvec& initialLogVolatilities,   // length-K vector of log-volatilities
  const unsigned int dimTheta,               // length of the parameter vector
  const arma::colvec& hyperParameters,       // hyperparameters and other auxiliary model parameters
  const arma::mat& support,                  // (par.size(), 2)-matrix containing the lower and upper bounds of the support of each parameter
  const unsigned int lower,                  // type of algorithm for updating the count-data likelihood (0: pseudo-marginal; 2: MCWM, 3: exact evaluation of (approximate) likelihood)
  const unsigned int nParticlesUpper,        // number of upper-level particles
  const unsigned int nParticlesLower,        // number of lower-level (i.e. "filtering") particles
  const unsigned int nMetropolisHastingsUpdates, // number of MH updates per particle and SMC step
  const unsigned int nMetropolisHastingsUpdatesFirst, // number of MH updates per particle and SMC step at the first stage if we use dual tempering
  const double essResamplingThresholdUpper,  // ESS-based resampling threshold for the upper-level SMC sampler
  const double essResamplingThresholdLower,  // ESS-based resampling threshold for the lower-level SMC filter
  const arma::colvec& smcParameters,         // additional parameters to be passed to the particle filter
  const arma::colvec& mcmcParameters,        // additional parameters to be passed to the MCMC kernel
  const bool useAdaptiveTempering,           // should we determine the tempering schedule adaptively according to the CESS?
  const bool useImportanceTempering,         // should we compute and return the importance-tempering weights?
  const bool useAdaptiveProposal,            // should we adapt the proposal scale of the MCMC kernel as in Peters at al. (2010)?
  const bool useAdaptiveProposalScaleFactor1, // should we also adapt the constant by which the sample covariance matrix is multiplied?
  const double cessTarget,                   // CESS target threshold (only used if useAdaptiveTempering == true).
  const arma::colvec& alpha,                 // manually specified tempering schedule (only used if useAdaptiveTempering == false)
  const arma::colvec& adaptiveProposalParameters, // parameters needed for the adaptive mixture proposal from Peters at al. (2010).
  const arma::colvec& rwmhSd,                // scaling of the random-walk Metropolis--Hastings proposals
  const unsigned int nCores                  // number of nCores used (currently, this is not implemented)
)
{
  
  // Starting the timer:
  clock_t t1,t2; // timing
  t1 = clock(); // start timer
  
  std::cout << "setting up RNG" << std::endl;
  
  std::mt19937 engine; 
  RngDerived<std::mt19937> rngDerived(engine);
  
  std::cout << "setting up observations" << std::endl;

  unsigned int nObservations = logExchangeRates.n_cols; // number of observations
  unsigned int nStepsLower = nObservations; // number of lower-level SMC steps
  Observations observations; // observations 
  observations.logExchangeRates_ = logExchangeRates;
  observations.logVolatilities_  = logVolatilities;
  observations.initialLogExchangeRates_ = initialLogExchangeRates;
  observations.initialLogVolatilities_  = initialLogVolatilities;
     

  /////////////////////////////////////////////////////////////////////////////
  // Model class.
  /////////////////////////////////////////////////////////////////////////////
      std::cout << "setting up Model class" << std::endl;
  
  Model<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations> model(rngDerived, hyperParameters, observations, nObservations, nCores);
  model.setSupport(support);
  model.setDimTheta(dimTheta);

  /////////////////////////////////////////////////////////////////////////////
  // Class for running the lower-level SMC algorithm.
  /////////////////////////////////////////////////////////////////////////////
  Smc<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations, Particle, Aux, SmcParameters> smc(
    rngDerived, model, nStepsLower,
    static_cast<SmcProposalType>(0), // 0: sample from prior; 5: lookahead
    essResamplingThresholdLower,
    static_cast<SmcBackwardSamplingType>(0),
    false,
    1,
    nCores
  );
  smc.setUseGaussianParametrisation(false);
  smc.setNParticles(nParticlesLower);
  smc.setNLookaheadSteps(smcParameters(1));
  smc.setSamplePath(false); // NOTE: we are not storing the paths at the moment!

  /////////////////////////////////////////////////////////////////////////////
  // Class for running MCMC algorithms.
  /////////////////////////////////////////////////////////////////////////////
  
  std::cout << "setting up MCMC class" << std::endl;
  
  Mcmc<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations, McmcParameters> mcmc(
    rngDerived, model, nCores
  );
  mcmc.setRwmhSd(rwmhSd);
  mcmc.setUseAdaptiveProposal(useAdaptiveProposal);
  mcmc.setUseDelayedAcceptance(false);
  mcmc.setAdaptiveProposalParameters(adaptiveProposalParameters);
  mcmc.setUseAdaptiveProposalScaleFactor1(useAdaptiveProposalScaleFactor1);
  mcmc.setIsWithinSmcSampler(true);
  
  /////////////////////////////////////////////////////////////////////////////
  // Class for running the upper-level SMC sampler.
  /////////////////////////////////////////////////////////////////////////////
  
  std::cout << "setting up SMC sampler class" << std::endl;
  
  SmcSampler<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations, Particle, Aux, SmcParameters, McmcParameters> smcSampler(rngDerived, model, smc, mcmc, nCores);
  smcSampler.setNParticles(nParticlesUpper);
  smcSampler.setEssResamplingThreshold(essResamplingThresholdUpper);
  smcSampler.setUseAdaptiveTempering(useAdaptiveTempering);
  smcSampler.setUseDoubleTempering(false);
  smcSampler.setLower(static_cast<SmcSamplerLowerType>(lower)); // use lower-level pseudo-marginal approach
  smcSampler.setNMetropolisHastingsUpdates(nMetropolisHastingsUpdates);
  smcSampler.setUseAdaptiveCessTarget(false);
  
  
  if (smcSampler.getUseAdaptiveTempering()) 
  {
    smcSampler.setCessTarget(cessTarget); // specify the sequence of temperatures (and the number of steps) adaptively based on the CESS.
  }
  else // i.e. if we specifiy the sequence of temperatures manually
  {
    smcSampler.setAlpha(alpha); // manually specify the sequence of temperatures (and in particular, the number of steps).
  }
  
  std::cout << "running the SMC sampler" << std::endl;
  
  smcSampler.runSmcSampler(); // running the SMC sampler
  
  if (useImportanceTempering)
  {
    smcSampler.computeImportanceTemperingWeights();
  }
  
  t2 = clock(); // stop timer 
  double cpuTime = (static_cast<double>(t2)-static_cast<double>(t1)) / CLOCKS_PER_SEC; // elapsed time in seconds
  std::cout << "Running the SMC sampler took " << cpuTime << " seconds." << std::endl;
  
  // Computing some additional output
  std::vector<ParticleUpper<LatentPath, Aux>> finalParticles(nParticlesUpper);
  smcSampler.getFinalParticles(finalParticles);
 
  
  if (useImportanceTempering)
  {
    return Rcpp::List::create(             
      Rcpp::Named("inverseTemperatures")                       = smcSampler.getAlpha(),
      Rcpp::Named("logUnnormalisedWeights")                    = smcSampler.getLogUnnormalisedWeightsFull(),
      Rcpp::Named("selfNormalisedWeights")                     = smcSampler.getSelfNormalisedWeightsFull(),  
      Rcpp::Named("theta")                                     = smcSampler.getThetaFull(), 
      Rcpp::Named("cpuTime")                                   = cpuTime,
      Rcpp::Named("logEvidenceEstimate")                       = smcSampler.getLogEvidenceEstimate(),
      Rcpp::Named("logEvidenceEstimateEssAlternate")           = smcSampler.getLogEvidenceEstimateEssAlternate(),
      Rcpp::Named("logEvidenceEstimateEssResampledAlternate")  = smcSampler.getLogEvidenceEstimateEssResampledAlternate(),
      Rcpp::Named("logUnnormalisedReweightedWeights")          = smcSampler.getLogUnnormalisedReweightedWeights(), 
      Rcpp::Named("logUnnormalisedReweightedWeightsResampled") = smcSampler.getLogUnnormalisedReweightedWeightsResampled(),
      Rcpp::Named("selfNormalisedReweightedWeights")           = smcSampler.getSelfNormalisedReweightedWeights(), 
      Rcpp::Named("selfNormalisedReweightedWeightsResampled")  = smcSampler.getSelfNormalisedReweightedWeightsResampled(),
      Rcpp::Named("selfNormalisedWeightsEss")                  = smcSampler.getSelfNormalisedWeightsEss(),
      Rcpp::Named("selfNormalisedWeightsEssResampled")         = smcSampler.getSelfNormalisedWeightsEssResampled(),
      Rcpp::Named("ess")                                       = smcSampler.getEss(),
      Rcpp::Named("essResampled")                              = smcSampler.getEssResampled(),
      Rcpp::Named("acceptanceRates")                           = smcSampler.getAcceptanceRates(),
      Rcpp::Named("maxParticleAutocorrelations")               = smcSampler.getMaxParticleAutocorrelations()
    );
  }
  else 
  {
    return Rcpp::List::create(
      Rcpp::Named("logEvidenceEstimate")                       = smcSampler.getLogEvidenceEstimate(),
      Rcpp::Named("logUnnormalisedWeights")                    = smcSampler.getLogUnnormalisedWeightsFull(),                        
      Rcpp::Named("selfNormalisedWeights")                     = smcSampler.getSelfNormalisedWeightsFull(),
      Rcpp::Named("inverseTemperatures")                       = smcSampler.getAlpha(),
      Rcpp::Named("theta")                                     = smcSampler.getThetaFull(), 
      Rcpp::Named("cpuTime")                                   = cpuTime,
      Rcpp::Named("acceptanceRates")                           = smcSampler.getAcceptanceRates(),
      Rcpp::Named("maxParticleAutocorrelations")               = smcSampler.getMaxParticleAutocorrelations()
    );
  }

}

////////////////////////////////////////////////////////////////////////////////
// Approximates the marginal (count-data) likelihood 
// via the SMC filter
////////////////////////////////////////////////////////////////////////////////
// [[Rcpp::depends("RcppArmadillo")]]
// [[Rcpp::export]]
Rcpp::List runSmcFilterCpp
(
  const arma::mat& logExchangeRates,         // (K, T)-matrix of log-exchange rates
  const arma::mat& logVolatilities,          // (K, T)-matrix of log-volatilities
  const arma::colvec& initialLogExchangeRates,  // length-K vector of log-exchange rates
  const arma::colvec& initialLogVolatilities,   // length-K vector of log-volatilities
  const unsigned int dimTheta,               // length of the parameter vector
  const arma::colvec& hyperParameters,       // hyperparameters and other auxiliary model parameters
  const arma::mat& support,                  // (par.size(), 2)-matrix containing the lower and upper bounds of the support of each parameter
  const unsigned int nParticles,             // number of particles per MCMC iteration within each lower-level SMC algorithm
  const double essResamplingThreshold,       // ESS-based resampling threshold for the lower-level SMC algorithms
  const arma::colvec& smcParameters,         // additional parameters to be passed to the particle filter
  const arma::colvec& thetaInit,             // initial value for theta (if we keep theta fixed throughout) 
  unsigned int smcProposalType,              // type of proposal kernel used by the SMC filter
  const unsigned int nCores                  // number of nCores used (currently, this is not implemented)
)
{

  std::mt19937 engine; 
  RngDerived<std::mt19937> rngDerived(engine);
  
//   std::cout << "setting up observations" << std::endl;
  unsigned int nObservations = logExchangeRates.n_cols; // number of observations
  unsigned int nSteps = nObservations; // number of lower-level SMC steps
  Observations observations; // observations 
  observations.logExchangeRates_ = logExchangeRates;
  observations.logVolatilities_  = logVolatilities;
  observations.initialLogExchangeRates_ = initialLogExchangeRates;
  observations.initialLogVolatilities_  = initialLogVolatilities;
     
  
  
//   std::cout << "setting up model class" << std::endl;
  // Model class.
  Model<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations> model(rngDerived, hyperParameters, observations, nObservations, nCores);
  model.setSupport(support);
  model.setDimTheta(dimTheta);
  
//   std::cout << "setting up SMC class" << std::endl;
  
  Smc<ModelParameters, LatentVariable, LatentPath, LatentPathRepar, Observations, Particle, Aux, SmcParameters> smc(
    rngDerived, model, nSteps,
    static_cast<SmcProposalType>(smcProposalType), 
    essResamplingThreshold,
    static_cast<SmcBackwardSamplingType>(1),
    false,
    1,
    nCores
  );
  smc.setUseGaussianParametrisation(false);
  smc.setNParticles(nParticles);
  smc.setNLookaheadSteps(smcParameters(0)); 
  smc.setSamplePath(false);
  
    // TODO: implement support for use of gradient information
  LatentPath latentPath;
  AuxFull<Aux> aux; // TODO: implement support for correlated psuedo-marginal approaches

//   std::cout << "running the SMC algorithm!" << std::endl;
  double logLikelihoodEstimate = smc.runSmc(smc.getNParticles(), thetaInit, latentPath, aux, 1.0);
  
//   std::cout << "finished runSmcFilterCpp()" << std::endl;
  
  return Rcpp::List::create(
      Rcpp::Named("logLikelihoodEstimate") = logLikelihoodEstimate,
      Rcpp::Named("trajectory") = latentPath
    );
}
