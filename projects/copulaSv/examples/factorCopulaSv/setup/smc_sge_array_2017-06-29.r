MODELS_AUX               <- c(0,1,2,3,3,3,5,5,5)
MODELS                   <- rep(rep(MODELS_AUX, times=3), times=2)
# Model to be estimated (needed for model comparison):
# 0: constant productivity rate.
# 1: productivity rates regressed on fDays,
# 2: direct density dependence (log-productivity rates specified as a linear function of abundance),
# 3: threshold dependence (of the productivity rate) on the observed heron counts (with nLevels+1 levels),
# 4: threshold dependence (of the productivity rate) on the true heron counts (with nLevels+1 levels),
# 5: latent Markov regime-switching dynamics for the productivity rates (with nLevels regimes).

N_LEVELS_AUX             <- c(2,2,2,2,3,4,2,3,4)
N_LEVELS                 <- rep(rep(N_LEVELS_AUX, times=3), times=2)
# Maximum number of levels in the step functions for the productivity parameter
# used by some models:

N_AGE_GROUPS_AUX         <- c(2,3,4)
N_AGE_GROUPS             <- rep(rep(N_AGE_GROUPS_AUX, each=length(MODELS_AUX)), times=2)
# Maximum number of age groups

N_CONFIGS                <- length(MODELS) # number of different model/algorithm configurations

CESS_TARGET              <- rep(0.9999, times=N_CONFIGS) # target conditional effective sample size used for adaptive tempering
CESS_TARGET_FIRST        <- rep(0.9999, times=N_CONFIGS) # target conditional effective sample size used for adaptive tempering at Stage 1 of the algorithm (if double tempering is used)
N_PARTICLES_UPPER        <- rep(1000, times=N_CONFIGS) # number of particles used by the SMC sampler
N_PARTICLES_LOWER        <- rep(4000, times=N_CONFIGS) # number of lower-level particles (i.e. number of particles used to approximate the marginal likelihood)

USE_IMPORTANCE_TEMPERING <- FALSE # should we also compute the weights and log-evidence estimates associated with an importance-tempering approach?
N_STEPS                  <- 100 # number of SMC steps (if adaptive tempering is not used)
SIMULATE_DATA            <- FALSE # should we use simulated data?

ALPHA                    <- seq(from=0, to=1, length=N_STEPS)
USE_DELAYED_ACCEPTANCE   <- 1 # should delayed acceptance be used?
USE_ADAPTIVE_TEMPERING   <- 1 # should we use adaptive tempering?
USE_ADAPTIVE_CESS_TARGET <- rep(0, times=N_CONFIGS)

USE_ADAPTIVE_PROPOSAL               <- rep(1, each=N_CONFIGS) # should we adapt the proposal covariance matrix along the lines of Peters et al. (2010)? (only adapt after the burnin phase)
USE_ADAPTIVE_PROPOSAL_SCALE_FACTOR1 <- rep(1, each=N_CONFIGS) # should we also adapt the constant by which the sample covariance matrix is multiplied?

USE_DOUBLE_TEMPERING     <- rep(1, times=N_CONFIGS) # should we temper the two likelihood components separately?
LOWER                    <- c(rep(3, times=N_CONFIGS/2), rep(0, times=N_CONFIGS/2)) # type of algorithm for updating the count-data likelihood (0: pseudo-marginal; 2: MCWM)

SMC_PARAMETERS           <- numeric(3) # additional parameters to be passed to the particle filter
MCMC_PARAMETERS          <- numeric(0) # additional parameters to be passed to the MCMC (e.g. PMMH) updates

MODEL_TYPE_NAMES         <- c(
  "Constant",
  "Regressed on number of frost days", 
  "Direct-density dependence", 
  "Threshold dependence", 
  "Threshold dependence (true counts)", 
  "Regime switching"
)

MODEL_NAMES <- c()
for (ii in 1:N_CONFIGS) {
  if (MODELS[ii] %in% c(3:4)) {
    MODEL_NAMES <- c(MODEL_NAMES, paste(MODEL_TYPE_NAMES[MODELS[ii]+1], " ($K=", N_LEVELS[ii], " $)", sep=''))
  } else if (MODELS[ii] %in% c(5)) {
    MODEL_NAMES <- c(MODEL_NAMES, paste(MODEL_TYPE_NAMES[MODELS[ii]+1], " ($K=", N_LEVELS[ii], " $)", sep=''))
  } else {
    MODEL_NAMES <- c(MODEL_NAMES, paste(MODEL_TYPE_NAMES[MODELS[ii]+1], sep=''))
  }
}
