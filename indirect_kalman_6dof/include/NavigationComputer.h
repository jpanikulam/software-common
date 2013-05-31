#ifndef NAVIGATIONCOMPUTER_H
#define NAVIGATIONCOMPUTER_H

#include "Kalman.h"
#include "INS.h"
#include "INSInitializer.h"

#include <boost/array.hpp>
#include <boost/optional.hpp>
#include <vector>

class NavigationComputer {
public:
    struct Config {
        double T_imu;
        double T_kalman;
        double T_kalman_correction;

        double y_a_max_norm_error;
        unsigned int y_a_log_size;

        Kalman::PredictConfig predict;
        Kalman::UpdateConfig update;
        INSInitializer::Config init;
    };

    NavigationComputer(const Config &config);

    struct Stats {
        unsigned int y_a_count;
        unsigned int y_m_count;
        unsigned int y_d_count;
        unsigned int y_z_count;
        double y_a_norm_error;
    };

    struct State {
        INS::State filt;
        Eigen::Matrix<double, 15, 15> cov;
        Stats stats;
    };

    boost::optional<State> getState() const;
    INS::Error getINSError() const;

    void updateINS(const INS::Measurement &measurement, double measurement_time);
    void updateMag(const Eigen::Vector3d &y_m, double measurement_time);
    void updateDVL(const Eigen::Matrix<double, 4, 1> &y_d,
                   const boost::array<bool, 4> &d_valid,
                   double measurement_time);
    void updateDepth(double y_z, double measurement_time);

    void run(double run_time);

private:
    // run sub-methods
    void tryInitINS(double run_time);
    void predict(int updates);
    void update();

    bool verifyKalmanTime(const std::string &sensor, double measurement_time);

    Config config;
    boost::optional<INS> ins;
    double ins_time;
    INSInitializer ins_init;

    Kalman kalman;
    double kalman_time;
    double last_correction_time;

    std::vector<Eigen::Vector3d> y_a_log;
    boost::optional<Eigen::Vector3d> y_m;
    Eigen::Vector4d y_d;
    boost::array<bool, 4> d_valid;
    boost::optional<double> y_z;

    Stats stats;
};

#endif
