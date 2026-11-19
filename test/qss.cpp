// Copyright (c) 2026 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <type_traits>
#include <vector>

#include <fmt/format.h>

#include <boost/ut.hpp>

static double e2u(const std::function<double(double)>& f,
                  double                               v0,
                  double                               v1,
                  double                               v2) noexcept
{
    double h = 1e-3;
    auto   F = [&](double e) { return f(v0 + v1 * e + v2 * e * e); };
    return (-F(2 * h) + 16 * F(h) - 30 * F(0) + 16 * F(-h) - F(-2 * h)) /
           (12 * h * h) / 2.0;
}

static double e2b(const std::function<double(double, double)>& f,
                  double                                       a0,
                  double                                       a1,
                  double                                       a2,
                  double                                       b0,
                  double                                       b1,
                  double                                       b2) noexcept
{
    double h = 1e-3;
    auto   F = [&](double e) {
        return f(a0 + a1 * e + a2 * e * e, b0 + b1 * e + b2 * e * e);
    };
    return (-F(2 * h) + 16 * F(h) - 30 * F(0) + 16 * F(-h) - F(-2 * h)) /
           (12 * h * h) / 2.0;
}

template<typename Dynamics>
static double e2u_sim(double                v0,
                      double                v1,
                      double                v2,
                      const irt::parameter& p = irt::parameter{}) noexcept
{
    irt::simulation sim;

    auto& m    = sim.alloc<Dynamics>();
    auto& sink = sim.alloc<irt::qss3_integrator>();

    p.copy_to(irt::get_model(m));
    (void)sim.connect(irt::get_model(m), 0, irt::get_model(sink), 0);
    m.value[0] = v0;
    m.value[1] = v1;
    m.value[2] = v2;

    const auto ret = m.lambda(sim);
    boost::ut::expect(boost::ut::fatal(ret.has_value()));

    return sim.active_output_messages.back()[2];
}

template<typename Dynamics>
static double e2b_sim(double a0,
                      double a1,
                      double a2,
                      double b0,
                      double b1,
                      double b2) noexcept
{
    irt::simulation sim;

    auto& m    = sim.alloc<Dynamics>();
    auto& sink = sim.alloc<irt::qss3_integrator>();

    (void)sim.connect(irt::get_model(m), 0, irt::get_model(sink), 0);
    m.values[0] = a0;
    m.values[2] = a1;
    m.values[4] = a2;
    m.values[1] = b0;
    m.values[3] = b1;
    m.values[5] = b2;

    const auto ret = m.lambda(sim);

    boost::ut::expect(not ret.has_error());

    return sim.active_output_messages.back()[2];
}

struct dated_value {
    double t;
    double value;
};

/// Runs @a sim until its end and returns the dates and the last values of the
/// messages received by @a cnt.
static std::vector<dated_value> run_and_record(irt::simulation&     sim,
                                               const irt::counter& cnt)
{
    std::vector<dated_value> ret;

    boost::ut::expect(boost::ut::fatal(sim.initialize().has_value()));

    do {
        const auto before = cnt.event_number;
        boost::ut::expect(boost::ut::fatal(sim.run().has_value()));
        if (cnt.event_number != before)
            ret.push_back({ sim.current_time(), cnt.last_value });
    } while (not sim.current_time_expired());

    return ret;
}

/// A QSS2 integrator of a constant @a slope from @a x0: its output is the
/// exact line @a x0 + @a slope * t after its first quantum.
static irt::qss2_integrator& alloc_qss2_line(irt::simulation& sim,
                                             const double     x0,
                                             const double     slope)
{
    auto& cst   = sim.alloc<irt::constant>();
    auto& integ = sim.alloc<irt::qss2_integrator>();
    sim.parameters[irt::get_index(sim.get_id(cst))].clear().set_constant(slope);
    sim.parameters[irt::get_index(sim.get_id(integ))]
      .clear()
      .set_integrator(x0, 0.001);
    boost::ut::expect(boost::ut::fatal(
      sim.connect_dynamics(cst, 0, integ, 0).has_value()));

    return integ;
}

/// An integrator of a constant slope 1 from 0, forced to 10 at t = 5, and
/// a constant @a late_slope sent at t = 5 when it is not zero.
template<typename Integrator>
static void check_integrator_reset(const double late_slope)
{
    using namespace boost::ut;

    const auto      slope_value = late_slope == 0.0 ? 1.0 : late_slope;
    irt::simulation sim;
    auto&           slope = sim.alloc<irt::constant>();
    auto&           force = sim.alloc<irt::constant>();
    auto&           integ = sim.alloc<Integrator>();
    auto&           cnt   = sim.alloc<irt::counter>();

    sim.parameters[irt::get_index(sim.get_id(slope))].clear().set_constant(
      slope_value, late_slope == 0.0 ? 0.0 : 5.0);
    sim.parameters[irt::get_index(sim.get_id(force))].clear().set_constant(
      10.0, 5.0);
    sim.parameters[irt::get_index(sim.get_id(integ))]
      .clear()
      .set_integrator(0.0, 0.1);
    sim.parameters[irt::get_index(sim.get_id(cnt))].clear();
    expect(fatal(sim.connect_dynamics(slope, 0, integ, 0).has_value()));
    expect(fatal(sim.connect_dynamics(force, 0, integ, 1).has_value()));
    expect(fatal(sim.connect_dynamics(integ, 0, cnt, 0).has_value()));
    sim.limits.set_bound(0, 8);

    const auto events = run_and_record(sim, cnt);

    expect(eq(integ.u, slope_value));
    expect(fatal(not events.empty()));
    expect(eq(events.front().t, 0.0));
    expect(eq(events.front().value, 0.0));
    expect(std::ranges::any_of(events, [](const dated_value& ev) {
        return ev.t == 5.0 and ev.value == 10.0;
    }));

    if constexpr (std::is_same_v<Integrator, irt::qss1_integrator>) {
        expect(gt(events.back().t, 7.8));
        expect(lt(std::abs(events.back().value - (10.0 + slope_value * 3.0)),
                  0.2));
    } else {
        expect(eq(events.back().t, 5.0));
        expect(eq(events.back().value, 10.0));
    }
}

/// A constant @a value sent at @a date to the port @a port of @a dst.
template<typename Dynamics>
static void send_at(irt::simulation& sim,
                    Dynamics&        dst,
                    const int        port,
                    const double     value,
                    const double     date)
{
    auto& cst = sim.alloc<irt::constant>();
    sim.parameters[irt::get_index(sim.get_id(cst))].clear().set_constant(value,
                                                                         date);
    boost::ut::expect(
      boost::ut::fatal(sim.connect_dynamics(cst, 0, dst, port).has_value()));
}

/// The value sent by a priority model which receives 1, 2 and 6 at the same
/// date on its first port, reduced by @a selection.
static double priority_select(const irt::priority_selection selection)
{
    using namespace boost::ut;

    irt::simulation sim;
    auto&           prio = sim.alloc<irt::qss1_priority_2>();
    auto&           cnt  = sim.alloc<irt::counter>();
    sim.parameters[irt::get_index(sim.get_id(prio))]
      .clear()
      .integers[0] = irt::ordinal(selection);
    send_at(sim, prio, 0, 1.0, 1.0);
    send_at(sim, prio, 0, 2.0, 1.0);
    send_at(sim, prio, 0, 6.0, 1.0);
    expect(fatal(sim.connect_dynamics(prio, 0, cnt, 0).has_value()));
    sim.limits.set_bound(0, 2);

    const auto events = run_and_record(sim, cnt);

    expect(fatal(eq(events.size(), 1u)));
    expect(eq(events.front().t, 1.0));
    return events.front().value;
}

int main()
{
    using namespace boost::ut;

    "new-qss3-lambda"_test = [] {
        const double v0 = 1.3, v1 = 0.7, v2 = 0.2;
        const double a0 = 1.3, a1 = 0.7, a2 = 0.2, b0 = 0.9, b1 = -0.4,
                     b2   = 0.3;
        int          fail = 0;

        auto row = [&](const char* n, double master, double fixed,
                       double oracle, double lambda) noexcept -> void {
            bool mok = std::fabs(master - oracle) < 1e-6,
                 fok = std::fabs(fixed - oracle) < 1e-6,
                 lok = std::fabs(lambda - oracle) < 1e-6;

            if (!fok)
                fail++;

            fmt::print("  {:<11} | oracle={:+10.6f} | old={:+10.6f} {:<4} | "
                       "new={:+10.6f} {:<4} | fix={:+10.6f} {}\n",
                       n, oracle, master, mok ? "OK?" : "FAIL", lambda,
                       lok ? "OK" : "FAIL", fixed, fok ? "OK" : "FAIL");
        };

        fmt::print("{:^13} | {:^17} | {:^22} | {:^22} | {:^22}\n", "modele",
                   "oracle(e^2)", "old qss3", "mew qss3 ", "fix");

        {
            double         n = 3;
            irt::parameter p;
            p.set_power(3);

            row("power",
                n * (n - 1) * std::pow(v0, n - 2) * (v1 * v1) +
                  n * std::pow(v0, n - 1) * v2,
                n * (n - 1) * std::pow(v0, n - 2) * (v1 * v1) / 2 +
                  n * std::pow(v0, n - 1) * v2,
                e2u([](double x) { return x * x * x; }, v0, v1, v2),
                e2u_sim<irt::qss3_power>(v0, v1, v2, p));
        }

        row("inverse", -(v2 / (v0 * v0)) + 2 * v1 * v1 / (v0 * v0 * v0),
            -(v2 / (v0 * v0)) + v1 * v1 / (v0 * v0 * v0),
            e2u([](double x) { return 1 / x; }, v0, v1, v2),
            e2u_sim<irt::qss3_inverse>(v0, v1, v2));

        row("log", -(v1 * v1) / (v0 * v0) + v2 / v0,
            -(v1 * v1) / (2 * v0 * v0) + v2 / v0,
            e2u([](double x) { return std::log(x); }, v0, v1, v2),
            e2u_sim<irt::qss3_log>(v0, v1, v2));

        row("exp", std::exp(v0) * (v1 * v1 + v2),
            std::exp(v0) * (v1 * v1 / 2 + v2),
            e2u([](double x) { return std::exp(x); }, v0, v1, v2),
            e2u_sim<irt::qss3_exp>(v0, v1, v2));

        row("sin", -std::sin(v0) * v1 * v1 + std::cos(v0) * v2,
            -std::sin(v0) * v1 * v1 / 2 + std::cos(v0) * v2,
            e2u([](double x) { return std::sin(x); }, v0, v1, v2),
            e2u_sim<irt::qss3_sin>(v0, v1, v2));

        row("cos", -std::cos(v0) * v1 * v1 - std::sin(v0) * v2,
            -std::cos(v0) * v1 * v1 / 2 - std::sin(v0) * v2,
            e2u([](double x) { return std::cos(x); }, v0, v1, v2),
            e2u_sim<irt::qss3_cos>(v0, v1, v2));

        row(
          "multiplier", a0 * b2 + 2 * a1 * b1 + a2 * b0,
          a0 * b2 + a1 * b1 + a2 * b0,
          e2b([](double a, double b) { return a * b; }, a0, a1, a2, b0, b1, b2),
          e2b_sim<irt::qss3_multiplier>(a0, a1, a2, b0, b1, b2));

        {
            double s   = std::sqrt(v0);
            double fp  = 1 / (2 * s);
            double fpp = -1 / (4 * v0 * s);

            row("sqrt", fpp * v1 * v1 + fp * v2, fpp * v1 * v1 / 2 + fp * v2,
                e2u([](double x) { return std::sqrt(x); }, v0, v1, v2),
                e2u_sim<irt::qss3_sqrt>(v0, v1, v2));
        }

        {
            double d   = 1 + v0 * v0;
            double fp  = 1 / d;
            double fpp = -2 * v0 / (d * d);
            row("atan", fpp * v1 * v1 + fp * v2, fpp * v1 * v1 / 2 + fp * v2,
                e2u([](double x) { return std::atan(x); }, v0, v1, v2),
                e2u_sim<irt::qss3_atan>(v0, v1, v2));
        }

        {
            double T   = std::tan(v0);
            double fp  = 1 + T * T;
            double fpp = 2 * T * fp;

            row("tan", fpp * v1 * v1 + fp * v2, fpp * v1 * v1 / 2 + fp * v2,
                e2u([](double x) { return std::tan(x); }, v0, v1, v2),
                e2u_sim<irt::qss3_tan>(v0, v1, v2));
        }

        {
            double t   = std::tanh(v0);
            double fp  = 1 - t * t;
            double fpp = -2 * t * fp;

            row("tanh", fpp * v1 * v1 + fp * v2, fpp * v1 * v1 / 2 + fp * v2,
                e2u([](double x) { return std::tanh(x); }, v0, v1, v2),
                e2u_sim<irt::qss3_tanh>(v0, v1, v2));
        }

        {
            double s   = 1 / (1 + std::exp(-v0));
            double fp  = s * (1 - s);
            double fpp = fp * (1 - 2 * s);

            row(
              "sigmoid", fpp * v1 * v1 + fp * v2, fpp * v1 * v1 / 2 + fp * v2,
              e2u([](double x) { return 1 / (1 + std::exp(-x)); }, v0, v1, v2),
              e2u_sim<irt::qss3_sigmoid>(v0, v1, v2));
        }

        {

            double c0 = 1 / b0, c1 = -b1 / (b0 * b0);
            double c2m = 2 * b1 * b1 / (b0 * b0 * b0) - b2 / (b0 * b0),
                   z2m = a0 * c2m + 2 * a1 * c1 + a2 * c0;
            double c2f = b1 * b1 / (b0 * b0 * b0) - b2 / (b0 * b0),
                   z2f = a0 * c2f + a1 * c1 + a2 * c0;

            row("division", z2m, z2f,
                e2b([](double a, double b) { return a / b; }, a0, a1, a2, b0,
                    b1, b2),
                e2b_sim<irt::qss3_division>(a0, a1, a2, b0, b1, b2));
        }

        {
            double y0 = a0, y1 = a1, y2 = a2, x0 = b0, x1 = b1, x2 = b2;
            double c0 = 1 / x0, c1 = -x1 / (x0 * x0), u0 = y0 * c0,
                   u1 = y1 * c0 + y0 * c1, d = 1 + u0 * u0, gp = 1 / d,
                   gpp   = -2 * u0 / (d * d);
            double c2m   = 2 * x1 * x1 / (x0 * x0 * x0) - x2 / (x0 * x0),
                   u2m   = y0 * c2m + 2 * y1 * c1 + y2 * c0,
                   arg3m = gpp * u1 * u1 + gp * u2m;
            double c2f   = x1 * x1 / (x0 * x0 * x0) - x2 / (x0 * x0),
                   u2f   = y0 * c2f + y1 * c1 + y2 * c0,
                   arg3f = gpp * u1 * u1 / 2 + gp * u2f;

            row("atan2", arg3m, arg3f,
                e2b([](double y, double x) { return std::atan2(y, x); }, a0, a1,
                    a2, b0, b1, b2),
                e2b_sim<irt::qss3_atan2>(a0, a1, a2, b0, b1, b2));
        }

        boost::ut::expect(not fail);
    };

    "new-qss2-3-observation"_test = [] {
        static const double two = 2, three = 3;

        auto cv3 = [](double m1, double m2, double m3, double m4,
                      double e) noexcept -> double {
            return m1 + m2 * e + m3 * e * e / two + m4 * e * e * e / three;
        };

        auto cv2 = [](double m1, double m2, double m3,
                      double e) noexcept -> double {
            return m1 + m2 * e + m3 * e * e / two;
        };

        int fail = 0;

        {
            double X = 1, u = .5, mu = .3, pu = .2;
            auto   T = [&](double t) {
                return X + u * t + mu / 2 * t * t + pu / 3 * t * t * t;
            };

            for (double e : { 0.3, 0.8 }) {
                double err = fabs(cv3(X, u, mu, pu, e) - T(e));
                if (err > 1e-9)
                    fail++;
                printf("e=%.1f err=%.0e ", e, err);
            }

            expect(not fail);
        }

        {
            double X = 1, u = .5, mu = .3;
            auto   T = [&](double t) { return X + u * t + mu / 2 * t * t; };

            for (double e : { 0.3, 0.8 }) {
                double err = fabs(cv2(X, u, mu, e) - T(e));
                if (err > 1e-9)
                    fail++;
                printf("e=%.1f err=%.0e ", e, err);
            }

            expect(not fail);
        }

        double v0 = 1.3, v1 = 0.7, v2 = 0.2;
        auto   x    = [&](double t) { return v0 + v1 * t + v2 * t * t; };
        auto   chk3 = [&](const char* n, std::function<double(double)> f,
                          double X, double u, double mu, double pu) {
            auto   Y     = [&](double t) { return f(x(t)); };
            double r2    = fabs(cv3(X, u, mu, pu, 0.2) - Y(0.2)),
                   r4    = fabs(cv3(X, u, mu, pu, 0.4) - Y(0.4));
            double ratio = r4 / r2;
            bool   ok    = ratio > 10 && ratio < 24;

            expect(ok);

            if (!ok)
                fail++;

            printf("  %-6s residu e=0.2:%.0e e=0.4:%.0e  ratio=%.1f %s\n", n,
                   r2, r4, ratio, ok ? "(ordre 3)" : "!!");
        };

        double s = std::sin(v0), c = std::cos(v0), ex = std::exp(v0),
               lo = std::log(v0);

        chk3(
          "power", [](double z) { return z * z * z; }, pow(v0, 3.),
          3 * v0 * v0 * v1, 6 * v0 * v1 * v1 + two * 3 * v0 * v0 * v2,
          three * 6 * v0 * v1 * v2 + 6 * v1 * v1 * v1 / two);

        chk3(
          "log", [](double z) { return std::log(z); }, lo, v1 / v0,
          -(v1 * v1) / (v0 * v0) + two * v2 / v0,
          -three * v1 * v2 / (v0 * v0) + v1 * v1 * v1 / (v0 * v0 * v0));

        chk3(
          "exp", [](double z) { return std::exp(z); }, ex, ex * v1,
          ex * (v1 * v1 + two * v2),
          ex * (three * v1 * v2 + v1 * v1 * v1 / two));

        chk3(
          "sin", [](double z) { return std::sin(z); }, s, c * v1,
          -s * v1 * v1 + two * c * v2,
          -three * s * v1 * v2 - c * v1 * v1 * v1 / two);

        chk3(
          "cos", [](double z) { return std::cos(z); }, c, -s * v1,
          -c * v1 * v1 - two * s * v2,
          -three * c * v1 * v2 + s * v1 * v1 * v1 / two);

        expect(not fail);
    };

    "compute-wake-up"_test = [] {
        expect(eq(irt::compute_wake_up(1.0, 0.0, 2.0), 0.5));
        expect(eq(irt::compute_wake_up(-1.0, 0.0, -4.0), 0.25));
        expect(irt::time_domain<irt::time>::is_infinity(
          irt::compute_wake_up(1.0, 0.0, -2.0)));
        expect(irt::time_domain<irt::time>::is_infinity(
          irt::compute_wake_up(1.0, 0.0, 0.0)));

        expect(eq(irt::compute_wake_up(1.0, 0.0, 2.0, 0.0), 0.5));
        expect(eq(irt::compute_wake_up(4.0, 0.0, 0.0, 1.0), 2.0));
        expect(eq(irt::compute_wake_up(3.0, 0.0, 2.0, 1.0), 1.0));
        expect(irt::time_domain<irt::time>::is_infinity(
          irt::compute_wake_up(1.0, 0.0, 0.0, -1.0)));
    };

    "qss2-quantizer-sends-each-level"_test = [] {
        irt::simulation sim;
        auto&           line = alloc_qss2_line(sim, 0.0, 4.0);
        auto&           qnt  = sim.alloc<irt::qss2_quantizer>();
        auto&           cnt  = sim.alloc<irt::counter>();
        sim.parameters[irt::get_index(sim.get_id(qnt))].reals[0] = 1.0;
        expect(fatal(sim.connect_dynamics(line, 0, qnt, 0).has_value()));
        expect(fatal(sim.connect_dynamics(qnt, 0, cnt, 0).has_value()));
        sim.limits.set_bound(0, 1.1);

        const auto events = run_and_record(sim, cnt);

        expect(fatal(eq(events.size(), 5u)));
        expect(eq(events[0].t, 0.0));
        for (std::size_t i = 1; i != events.size(); ++i) {
            const auto level = static_cast<double>(i);
            expect(lt(std::abs(events[i].t - (level - 0.5) / 4.0), 1e-9));
            expect(eq(events[i].value, level));
        }
    };

    "qss2-cross-sends-each-direction-on-its-port"_test = [] {
        using cross = irt::qss2_cross;

        irt::simulation sim;
        auto&           line = alloc_qss2_line(sim, -1.0, 1.0);
        auto&           crs  = sim.alloc<cross>();
        auto&           up   = sim.alloc<irt::counter>();
        auto&           down = sim.alloc<irt::counter>();
        sim.parameters[irt::get_index(sim.get_id(crs))].clear().set_cross(
          0.0, 5.0, 7.0);
        expect(fatal(
          sim.connect_dynamics(line, 0, crs, cross::port_value).has_value()));
        expect(fatal(
          sim.connect_dynamics(crs, cross::port_up, up, 0).has_value()));
        expect(fatal(
          sim.connect_dynamics(crs, cross::port_down, down, 0).has_value()));
        sim.limits.set_bound(0, 3);

        std::vector<dated_value> ups, downs;
        expect(fatal(sim.initialize().has_value()));
        do {
            const auto up_before   = up.event_number;
            const auto down_before = down.event_number;
            expect(fatal(sim.run().has_value()));
            if (up.event_number != up_before)
                ups.push_back({ sim.current_time(), up.last_value });
            if (down.event_number != down_before)
                downs.push_back({ sim.current_time(), down.last_value });
        } while (not sim.current_time_expired());

        expect(fatal(eq(downs.size(), 1u)));
        expect(eq(downs[0].t, 0.0));
        expect(eq(downs[0].value, 7.0));
        expect(fatal(eq(ups.size(), 1u)));
        expect(lt(std::abs(ups[0].t - 1.0), 1e-9));
        expect(eq(ups[0].value, 5.0));
    };

    "cross-copy-keeps-the-output-values"_test = [] {
        irt::qss3_cross src;
        src.output_values = { 5.0, 7.0 };
        src.threshold     = 2.0;

        const irt::qss3_cross copy(src);

        expect(eq(copy.output_values[0], 5.0));
        expect(eq(copy.output_values[1], 7.0));
        expect(eq(copy.threshold, 2.0));
    };

    "qss2-compare-sends-the-value-after-the-crossing"_test = [] {
        for (const auto x0 : { -1.0, -0.7, -0.3 }) {
            irt::simulation sim;
            auto&           line = alloc_qss2_line(sim, x0, 1.0);
            auto&           cmp  = sim.alloc<irt::qss2_compare>();
            auto&           cnt  = sim.alloc<irt::counter>();
            sim.parameters[irt::get_index(sim.get_id(cmp))]
              .clear()
              .set_compare(1.0, 0.0);
            expect(fatal(sim.connect_dynamics(line, 0, cmp, 0).has_value()));
            expect(fatal(sim.connect_dynamics(cmp, 0, cnt, 0).has_value()));
            sim.limits.set_bound(0, 3);

            const auto events = run_and_record(sim, cnt);

            expect(fatal(eq(events.size(), 2u)));
            expect(eq(events[0].t, 0.0));
            expect(eq(events[0].value, 0.0));
            expect(lt(std::abs(events[1].t + x0), 1e-9));
            expect(eq(events[1].value, 1.0));
        }
    };

    "generator-confluent-multiplies-the-new-period"_test = [] {
        irt::simulation sim;
        auto&           gen = sim.alloc<irt::generator>();
        auto&           cst = sim.alloc<irt::constant>();
        auto&           cnt = sim.alloc<irt::counter>();
        sim.parameters[irt::get_index(sim.get_id(gen))]
          .clear()
          .set_generator_ta(1.0);
        sim.parameters[irt::get_index(sim.get_id(cst))].clear().set_constant(
          2.0, 1.0);
        expect(fatal(sim.connect_dynamics(cst, 0, gen, irt::generator::x_mult_tr)
                       .has_value()));
        expect(fatal(sim.connect_dynamics(gen, 0, cnt, 0).has_value()));
        sim.limits.set_bound(0, 5.5);

        const auto events = run_and_record(sim, cnt);

        expect(fatal(eq(events.size(), 4u)));
        expect(eq(events[0].t, 1.0));
        expect(eq(events[1].t, 3.0));
        expect(eq(events[2].t, 4.0));
        expect(eq(events[3].t, 5.0));
    };

    "generator-external-keeps-the-remaining-time"_test = [] {
        irt::simulation sim;
        auto&           gen = sim.alloc<irt::generator>();
        auto&           cst = sim.alloc<irt::constant>();
        auto&           cnt = sim.alloc<irt::counter>();
        sim.parameters[irt::get_index(sim.get_id(gen))]
          .clear()
          .set_generator_ta(1.0);
        sim.parameters[irt::get_index(sim.get_id(cst))].clear().set_constant(
          -1.0, 0.5);
        expect(fatal(
          sim.connect_dynamics(cst, 0, gen, irt::generator::x_t).has_value()));
        expect(fatal(sim.connect_dynamics(gen, 0, cnt, 0).has_value()));
        sim.limits.set_bound(0, 2.5);

        const auto events = run_and_record(sim, cnt);

        expect(fatal(eq(events.size(), 2u)));
        expect(eq(events[0].t, 1.0));
        expect(eq(events[1].t, 2.0));
    };

    "integrator-sends-its-initial-value"_test = [] {
        auto check = []<typename Integrator>(Integrator*) {
            irt::simulation sim;
            auto&           integ = sim.alloc<Integrator>();
            auto&           cnt   = sim.alloc<irt::counter>();
            sim.parameters[irt::get_index(sim.get_id(integ))]
              .clear()
              .set_integrator(0.25, 0.1);
            expect(fatal(sim.connect_dynamics(integ, 0, cnt, 0).has_value()));
            sim.limits.set_bound(0, 1);

            const auto events = run_and_record(sim, cnt);

            expect(fatal(eq(events.size(), 1u)));
            expect(eq(events.front().value, 0.25));
        };

        check(static_cast<irt::qss1_integrator*>(nullptr));
        check(static_cast<irt::qss2_integrator*>(nullptr));
        check(static_cast<irt::qss3_integrator*>(nullptr));
    };

    "integrator-reset-keeps-the-derivative"_test = [] {
        check_integrator_reset<irt::qss1_integrator>(0.0);
        check_integrator_reset<irt::qss2_integrator>(0.0);
        check_integrator_reset<irt::qss3_integrator>(0.0);
    };

    "integrator-reset-takes-the-simultaneous-derivative"_test = [] {
        check_integrator_reset<irt::qss1_integrator>(2.0);
        check_integrator_reset<irt::qss2_integrator>(2.0);
        check_integrator_reset<irt::qss3_integrator>(2.0);
    };

    "priority-forwards-the-last-message"_test = [] {
        irt::simulation sim;
        auto&           prio = sim.alloc<irt::qss1_priority_2>();
        auto&           cnt  = sim.alloc<irt::counter>();
        sim.parameters[irt::get_index(sim.get_id(prio))].clear();
        send_at(sim, prio, 0, 1.0, 1.0);
        send_at(sim, prio, 1, 2.0, 2.0);
        send_at(sim, prio, 0, 3.0, 3.0);
        expect(fatal(sim.connect_dynamics(prio, 0, cnt, 0).has_value()));
        sim.limits.set_bound(0, 4);

        const auto events = run_and_record(sim, cnt);

        expect(fatal(eq(events.size(), 3u)));
        for (std::size_t i = 0; i != events.size(); ++i) {
            expect(eq(events[i].t, static_cast<double>(i + 1)));
            expect(eq(events[i].value, static_cast<double>(i + 1)));
        }
    };

    "priority-first-port-wins-on-simultaneous-messages"_test = [] {
        irt::simulation sim;
        auto&           prio = sim.alloc<irt::qss1_priority_3>();
        auto&           cnt  = sim.alloc<irt::counter>();
        sim.parameters[irt::get_index(sim.get_id(prio))].clear();
        send_at(sim, prio, 2, 9.0, 1.0);
        send_at(sim, prio, 1, 5.0, 1.0);
        send_at(sim, prio, 2, 7.0, 2.0);
        expect(fatal(sim.connect_dynamics(prio, 0, cnt, 0).has_value()));
        sim.limits.set_bound(0, 3);

        const auto events = run_and_record(sim, cnt);

        expect(fatal(eq(events.size(), 2u)));
        expect(eq(events[0].value, 5.0));
        expect(eq(events[1].value, 7.0));
    };

    "priority-reduces-the-simultaneous-messages-of-a-port"_test = [] {
        expect(eq(priority_select(irt::priority_selection::maximum), 6.0));
        expect(eq(priority_select(irt::priority_selection::minimum), 1.0));
        expect(eq(priority_select(irt::priority_selection::mean), 3.0));
    };

    "priority-qss2-forwards-the-slope"_test = [] {
        irt::simulation sim;
        auto&           line = alloc_qss2_line(sim, 0.0, 4.0);
        auto&           prio = sim.alloc<irt::qss2_priority_2>();
        sim.parameters[irt::get_index(sim.get_id(prio))].clear();
        expect(fatal(sim.connect_dynamics(line, 0, prio, 1).has_value()));
        sim.limits.set_bound(0, 1);

        expect(fatal(sim.initialize().has_value()));
        do {
            expect(fatal(sim.run().has_value()));
        } while (not sim.current_time_expired());

        expect(eq(prio.value[1], 4.0));
    };
}
