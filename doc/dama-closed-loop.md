# DEVS-DAMA closed loop

The experiment `closed_loop: the experiment of the article` of
`test/dama.cpp` couples an environment, a M/M/1 queue with a burst of
arrivals, to a DEVS-DAMA agent built with Irritator components only. The C++
code builds the model, runs the replications and reads the state of the
environment queue after each step; it holds no logic of the agent.

Run it with `IRRITATOR_DAMA_CLOSED_LOOP_CSV=<file>`; without the variable it
is skipped. The test `closed_loop: the agent switches to mu2 during the
burst` always runs one replication to check the wiring.

## Models and couplings

```
 Environment
   gen_a  Exp(1) x 2.5 ----------------------+--> queue.in      queue: single_server_queue
   gen_b  Exp(1) x 2.5 (on [500, 1000)) -----+      ^ rate      Exp(1) / rate, starts at mu1
     ^ x_t: stop_b0 (1e9 at 0), restart_b (Exp(0.4) at 500), stop_b1 (1e9 at 1000)

 Agent
   gen_a, gen_b --> omega_b.x0
   omega_b.y0 (belief) --> c.x0      omega_b.y1 (p_w) --> c.x1
   c.y0 (period) --> psi_delta.x[2 + lambda]
   gen_a, gen_b --> count.x0      queue.y0 --> count.x1
   count.y0 (n_B) --> psi_delta.x[2 + n-b]
   clock (period T_d = 4, value d_r = 0.1) --> psi_delta.x0 (init), x1 (run)
   psi_delta.y[mu] --> queue.rate, decisions counter
```

- **Arrivals.** Two Poisson processes of rate 0.4 superpose to the
  piecewise process 0.4, 0.8 on [500, 1000), then 0.4. B is stopped at
  t = 0 by a constant on its `x_t` port. At t = 500, a generator whose time
  advance reads the constant source {500, 1e9} and whose value is Exp(0.4)
  restarts B after an exponential delay, so B stays a Poisson process. A
  second constant stops B at t = 1000. A constant must be finite: 1e9 means
  never.
- **Queue.** The `rate` port (x[1]) applies the chosen mu to the services
  not started: the waiting clients keep their draws, scaled by the ratio of
  the rates.

## The components of the agent

- **Psi + Delta**: the `simulation_wrapper` of the supervisor of
  `run_supervisor`: mu in {0.6, 1.2} linked to the cost {0, 3}, criteria
  `time-mean` on the number of clients and on the cost, `weighted_sum` with
  `norm none` and the weights (0.6, 0.4), R = 10 replications, horizon
  h = 50, lambda injected as a period (prior 2.5). Restart and project: the
  clock sends init and run in the same bag, the `complete` run type sends
  the results after the value of the run message, d_r = 0.1.
- **n_B** (HSM, register i1): counts the arrivals (x0) and the departures
  (x1) of the environment and sends the number of clients believed present
  after each change. In the projected model, the constant `n-b` (a factor,
  its value is n_B) sends n_B at t = 0 to the `batch` port of the queue: the
  projections start with n_B clients, their services drawn as the others.
  Configuration `dama_empty` keeps the old projections from an empty queue.
- **Omega_B** (HSM, registers r1 belief, i1 count): arms a timer of
  W = 25 (about 10 arrivals at lambda = 0.4), counts the arrivals in the
  `else` branch of the waiting state (the timer keeps running), then at the
  wake up: no arrival, a new window; otherwise p_w = W / count goes out on
  y1, and the belief is replaced by p_w if |p_w - belief| > epsilon_0 = 0.25
  (filtered revision, the chain of `omega_b_revision_cycle`). The belief goes
  out on y0 after each window.
- **C with Omega_theta** (HSM, register r1 slow estimate): receives the
  belief and p_w in the same bag, moves the slow estimate
  p_hat += eta_w (p_w - p_hat), fuses 0.8 belief + 0.2 p_hat, bounds the
  result in [1/5, 1/0.05] (Theta) and sends it to the factor lambda. With
  two real registers, 0.2 p_hat is added by scaling r1, adding it and
  dividing it back.

## Simplifications

- **No clock in a HSM.** An inter-arrival cannot be measured: Omega_B uses
  the count of a window of fixed duration instead of the last 10 arrivals,
  and Omega_theta moves once per window with eta_w = 1 - 0.95^10 = 0.40,
  the weight of 10 arrivals with eta = 0.05.
- **Omega_theta lives in the HSM of C**: it needs p_w and its own register;
  a separate HSM would reach C one bag after the belief.
- **Omega_pi is not implemented**: T_d = 4 stays fixed. The observed L of an
  episode is a time integral, out of reach of a HSM without clock.
- **The client in service of the environment** is projected as a new
  client: its remaining service is drawn again (exact for an exponential
  service).
- **Common random numbers through time**: the replication r of every
  decision uses the same seed; the decisions of a run are correlated.

## Baselines and ablation

| configuration | construction |
|---|---|
| `mu1`, `mu2` | no agent, constant rate |
| `hysteresis_up_down` | a HSM counts n (arrivals on x0, departures on x1) and the mode: mu2 if n > up, back to mu1 if n < down; (3, 1), (5, 2) and (8, 3) |
| `dama_empty` | projections from an empty queue |
| `dama_W10`, `dama_W50` | window W of Omega_B of 10 and 50 |
| `no_omega_b` | epsilon_0 = 1e9: the belief stays at 2.5 |
| `no_omega_theta` | eta_w = 0: the slow estimate stays at 2.5 |
| `no_bounds` | the bounds of C are [0, 1e30] |

## Measures

For each replication of 2000: L in total and by phase, the fraction of time
in mu2, the cost 3 x fraction, U = 0.6 L + 0.4 cost, the switches of mode,
the delay from t = 500 to the first time in mu2 and from t = 1000 to the
first time in mu1 (0 if already there), the decisions and the computation
time. The seeds 1 to 30 are the same for all the configurations. The
campaign ends with the paired differences of U with the best hysteresis.

## Threshold policies as candidate actions (`dama_policy`)

The actions of the wrapper are threshold pairs (n_up, n_down) applied at
each event by a policy HSM, in the projected model and in the environment,
until the next decision:

- **policy** (HSM, i1 = n, i2 = mode 0 mu1, 1 mu2, -1 unknown): n arrives on
  x0, the thresholds on x2 and x3. Each message evaluates the policy: mu2 if
  n > n_up and the mode is not mu2, mu1 if n < n_down and the mode is not
  mu1. The comparisons are strict: (1e9, 1e9) is always mu1, (-1, -2)
  always mu2. n comes from a counter HSM: a HSM has 4 ports and could not
  tell a departure from new thresholds, the bits of the ports stay set.
- **projected model**: the arrivals (factor lambda), the queue, the
  constant `n-b` (batch of the queue and initial value of the counter on
  its x2), the constant `mode0` (the current rate of the real queue, on the
  rate port), the constants `n-up` and `n-down` (linked factors: (1e9,
  1e9), (-1, -2), (3, 1), (5, 2), (8, 3)), a counter HSM, the policy HSM
  (mode unknown at start: the queue keeps `mode0` between the thresholds)
  and a counter `rate` in `last_value` fed by `mode0` and the policy.
- **criteria**: the time means of n (weight 0.6) and of the rate
  (weight 2): 0.6 L + 2 mean(mu) = U + 1.2, since the cost is
  3 (mean(mu) - 0.6) / 0.6.
- **environment**: the counter n_B feeds the real policy and the factor
  `n-b`, the real policy feeds the rate of the queue and the factor
  `mode0`, the outputs `n-up` and `n-down` of the wrapper feed the ports x2
  and x3 of the real policy (always mu1 until the first decision).
- 5 policies x 10 replications: 50 sub-simulations by decision.

Configurations: `dama_policy` (T_d = 4), `dama_policy_T16` (T_d = 16) and
`dama_policy_no_omega_b` (epsilon_0 = 1e9). The CSV gives the choices of the
threshold pairs by phase (columns `choices_*`: mu1, mu2, (3,1), (5,2),
(8,3)).
