#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector [Px, Py, Velcity, Yaw, Yawd]
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  // ------------------Turned this------------------
  std_a_ = 1;

  // Process noise standard deviation yaw acceleration in rad/s^2
  // ------------------Turned this------------------
  std_yawdd_ = 0.5;
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  // As first process measurement come, initialized UKF
  is_initialized_ = false;

  // Initialize State Covariance Matrix P
  // ------------------Turned initial value------------------
  P_ << 0.1,   0, 0, 0, 0,
          0, 0.1, 0, 0, 0,
          0,   0, 1, 0, 0,
          0,   0, 0, 1, 0,
          0,   0, 0, 0, 1;


  // Set state dimension
  n_x_ = 5;

  // Set augmented dimension
  n_aug_ = 7;

  // Set spreading parameter
  lambda_ = 3 - n_aug_;

  // Create matrix for sigma points
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);

  // Create Vector for weights
  weights_ = VectorXd(2 * n_aug_ + 1);
  double weight_0 = lambda_ / (lambda_ + n_aug_);
  weights_(0) = weight_0;
  //2n+1 weights
  for(int i = 1; i < 2 * n_aug_ + 1; i++)
  {
      double weight = 0.5 / (n_aug_ + lambda_);
      weights_(i) = weight;
  }

  // Set Masurement noises matrix R
  R_laser_ = MatrixXd(2,2);
  R_laser_ << std_laspx_*std_laspx_,                     0,
                                  0, std_laspy_*std_laspy_;

  R_radar_ = MatrixXd(3,3);
  R_radar_ << std_radr_*std_radr_,                       0,                     0,
                                0, std_radphi_*std_radphi_,                     0,
                                0,                       0, std_radrd_*std_radrd_;

  // Set Process Noise Covariance Matrix
  Q = MatrixXd(2,2);
  Q << std_a_*std_a_,                     0,
                   0, std_yawdd_*std_yawdd_;
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  if(!is_initialized_)
  {
    // Switch between lidar and radar
    if(meas_package.sensor_type_ == MeasurementPackage::RADAR)
    {
      double rho = meas_package.raw_measurements_(0);
      double phi = meas_package.raw_measurements_(1);
      double rho_d = meas_package.raw_measurements_(2);
      double px = rho * cos(phi);
      double py = rho * sin(phi);
      double yawd = 0.0;
      // ------------------Turned initial value------------------
      x_ << px, py, 4, 0.5, 0;
    }
    else if(meas_package.sensor_type_ == MeasurementPackage::LASER)
    {
      double px = meas_package.raw_measurements_(0);
      double py = meas_package.raw_measurements_(1);
      // ------------------Turned initial value------------------
      x_ << px, py, 4, 0.5, 0.0;
    }

    is_initialized_ = true;
    time_us_ = meas_package.timestamp_;

    return;
  }

  // Calculate delta t
  double delta_t = (meas_package.timestamp_ - time_us_) / 1000000.0;
  time_us_ = meas_package.timestamp_;

  // Motion Update
  Prediction(delta_t);

  // Measurement Update
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
  {
    UpdateRadar(meas_package);
  }
  else
  {
    UpdateLidar(meas_package);
  }
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  /********************************************
  ************Generate Sigma points************
  ********************************************/
  lambda_ = 3 - n_x_;
  //create sigma point matrix
  MatrixXd Xsig_ = MatrixXd(n_x_, 2 * n_x_ + 1);

  Xsig_.col(0) = x_;
  //calculate square root of P
  MatrixXd A = P_.llt().matrixL();
  for(int i = 0; i < n_x_; i++)
  {
    Xsig_.col(i+1) = x_ + std::sqrt(lambda_+n_x_) * A.col(i);
    Xsig_.col(i+1+n_x_) = x_ - std::sqrt(lambda_+n_x_) * A.col(i);
  }

  /********************************************
  ************Augmented Sigma points************
  ********************************************/
  lambda_ = 3 - n_aug_;
  // Augmented Sigma Points
  VectorXd x_aug = VectorXd(n_aug_);
  x_aug.fill(0.0);

  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
  P_aug.fill(0.0);

  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
  Xsig_aug.fill(0.0);

  // create augmented mean state
  x_aug.head(n_x_) = x_;
  P_aug.topLeftCorner(n_x_,n_x_) = P_;
  P_aug.bottomRightCorner(2, 2) = Q;

  // calculate square root of P_aug
  MatrixXd A_aug = P_aug.llt().matrixL();

  // create augmented sigma points
  Xsig_aug.col(0) = x_aug;
  for(int i=0; i < n_aug_; i++)
  {
    Xsig_aug.col(i+1) = x_aug + sqrt(lambda_ + n_aug_) * A_aug.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * A_aug.col(i);
  }

  /*******************************************
  ************Predict Sigma points************
  ********************************************/
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);
  for(int i=0; i <2 * n_aug_ + 1; i++)
  {
    double px = Xsig_aug(0, i);
    double py = Xsig_aug(1, i);
    double v = Xsig_aug(2, i);
    double phi = Xsig_aug(3, i);
    double phi_dot = Xsig_aug(4, i);
    double noise_Vol_acc = Xsig_aug(5, i);
    double noise_Yal_acc = Xsig_aug(6, i);
    // avoid division by zero
    if(phi_dot > 0.001)
      {
          Xsig_pred_(0,i) = Xsig_aug(0,i) + (v/phi_dot)*(sin(phi+phi_dot*delta_t)-sin(phi)) + 0.5*delta_t*delta_t*cos(phi)*noise_Vol_acc;
          Xsig_pred_(1,i) = Xsig_aug(1,i) + (v/phi_dot)*(-1*cos(phi+phi_dot*delta_t)+cos(phi)) + 0.5*delta_t*delta_t*sin(phi)*noise_Vol_acc;
          Xsig_pred_(2,i) = Xsig_aug(2,i) + 0 + delta_t*noise_Vol_acc;
          Xsig_pred_(3,i) = Xsig_aug(3,i) + phi_dot*delta_t + 0.5*delta_t*delta_t*noise_Yal_acc;
          Xsig_pred_(4,i) = Xsig_aug(4,i) + 0 + delta_t*noise_Yal_acc;
      }
      else
      {
          Xsig_pred_(0,i) = Xsig_aug(0,i) + v*cos(phi)*delta_t + 0.5*delta_t*delta_t*cos(phi)*noise_Vol_acc;
          Xsig_pred_(1,i) = Xsig_aug(1,i) + v*sin(phi)*delta_t + 0.5*delta_t*delta_t*sin(phi)*noise_Vol_acc;
          Xsig_pred_(2,i) = Xsig_aug(2,i) + 0 + delta_t*noise_Vol_acc;
          Xsig_pred_(3,i) = Xsig_aug(3,i) + phi_dot*delta_t + 0.5*delta_t*delta_t*noise_Yal_acc;
          Xsig_pred_(4,i) = Xsig_aug(4,i) + 0 + delta_t*noise_Yal_acc;
      }
  }
  /*********************************************
  ****Predict Mean & Covariance state matrix****
  **********************************************/
  // Predict mean state matrix
  x_.fill(0.0);
  for(int i = 0; i < 2 * n_aug_ + 1; i++)
  {
      x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }
  // Predicted state covariance matrix
  P_.fill(0.0);
  for(int i = 0; i < 2 * n_aug_ + 1; i++)
  {
      VectorXd x_diff = Xsig_pred_.col(i) - x_;

      // normalize angle
      x_diff(3) = remainder(x_diff(3), 2.0 * M_PI);

      P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
  }
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  /***********************************************
  ****initialize matrix for futher calculation****
  ************************************************/
  // Lidar measurement dimension (px, py)
  int n_z = 2;
  
  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
  Zsig.fill(0.0);

  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);

  //measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);

  /****************************************************
  ****transform sigma points into measurement space****
  *****************************************************/
  for(int i = 0; i < 2 * n_aug_ + 1; i++)
  {
    double px = Xsig_pred_(0, i);
    double py = Xsig_pred_(1, i);
    
    Zsig.col(i) << px,
                   py;
  }

  /************************************************
  ****calculate mean predict measurement z_pred****
  *************************************************/
  for(int i = 0; i < 2* n_aug_ + 1; i++)
  {
    z_pred += weights_(i) * Zsig.col(i);
  }

  /************************************************
  ****calculate measurement covariance matrix S****
  *************************************************/
  for(int i = 0; i < 2 * n_aug_ + 1; i++)
  {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    S += weights_(i) * z_diff * z_diff.transpose();
  }
  
  /****************************
  ****Add measurement noise****
  *****************************/
  S += R_laser_;
  
  /********************************************
  ****calculate cross correlation matrix Tc****
  *********************************************/
  MatrixXd Tc = MatrixXd(n_x_, n_z);
  Tc.fill(0.0);

  for(int i = 0; i < 2 * n_aug_ + 1; i++)
  {
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    
    //normalize angles
    if(x_diff(3) > M_PI)
    {
      x_diff(3) -= 2. * M_PI;
    }
    else if(x_diff(3) < -M_PI)
    {
      x_diff(3) += 2. * M_PI;
    }

    VectorXd z_diff = Zsig.col(i) - z_pred;

    Tc += weights_(i) * x_diff * z_diff.transpose();
  }
  
  /*************************
  ****Find Kalman gain K****
  **************************/
  MatrixXd K = Tc * S.inverse();

  /*********************************************************
  ****calculate residule from incoming laser measurement****
  **********************************************************/
  VectorXd z = VectorXd(n_z);
  double px = meas_package.raw_measurements_(0);
  double py = meas_package.raw_measurements_(1);
  z << px,
       py;
  // residuale
  VectorXd z_diff = z - z_pred;

  /**************************
  ****calculate NIS laser****
  ***************************/
  NIS_laser_ = z_diff.transpose() * S.inverse() * z_diff;
  // cout << "NIS_laser: " << NIS_laser_ << endl;
  
  /***************************************
  ****update state mean and covariance****
  ****************************************/
  x_ += K * z_diff;
  P_ -= K * S* K.transpose();
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  /***********************************************
  ****initialize matrix for futher calculation****
  ************************************************/
  //radar measurement dimension rho, phi, and r_dot
  int n_z = 3;
  
  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
  Zsig.fill(0.0);

  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);

  //measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);

  double rho = 0;
  double phi = 0;
  double rho_d = 0;

  /****************************************************
  ****transform sigma points into measurement space****
  *****************************************************/
  for(int i = 0; i < 2 * n_aug_ + 1; i++)
  {
    double px    = Xsig_pred_(0, i);
    double py    = Xsig_pred_(1, i);
    double v     = Xsig_pred_(2, i);
    double yaw   = Xsig_pred_(3, i);
    double yaw_d = Xsig_pred_(4, i);
    
    rho = sqrt(px*px+py*py);
    phi = atan2(py,px);
    rho_d = (px*cos(yaw)*v+py*sin(yaw)*v) / rho;
    
    Zsig.col(i) << rho,
                   phi,
                   rho_d;
  }

  /************************************************
  ****calculate mean predict measurement z_pred****
  *************************************************/
  for(int i = 0; i < 2 * n_aug_ + 1; i++)
  {
    z_pred += weights_(i) * Zsig.col(i);
  }

  /************************************************
  ****calculate measurement covariance matrix S****
  *************************************************/
  for(int i = 0; i < 2 * n_aug_ + 1; i++)
  {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    // normalize angle
    if(z_diff(1) > M_PI)
    {
      z_diff(1) -= 2. * M_PI;
    }
    else if(z_diff(1) < - M_PI)
    {
      z_diff(1) += 2. * M_PI;
    }
    S += weights_(i) * z_diff * z_diff.transpose();
  }
  
  /****************************
  ****Add measurement noise****
  *****************************/
  S += R_radar_;
  
  /********************************************
  ****calculate cross correlation matrix Tc****
  *********************************************/
  MatrixXd Tc = MatrixXd(n_x_, n_z);
  Tc.fill(0.0);
  
  for(int i = 0; i < 2 * n_aug_ + 1; i++)
  {
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //normalize angles
    if(x_diff(3) > M_PI)
    {
      x_diff(3) -= 2. * M_PI;
    }
    else if(x_diff(3) < -M_PI)
    {
      x_diff(3) += 2. * M_PI;
    }
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //normalize angles
    if(z_diff(1) > M_PI)
    {
      z_diff(1) -= 2. * M_PI;
    }
    else if(z_diff(1) < -M_PI)
    {
      z_diff(1) += 2. * M_PI;
    }

    Tc += weights_(i) * x_diff * z_diff.transpose();
  }

  /*************************
  ****Find Kalman gain K****
  **************************/
  MatrixXd K = Tc * S.inverse();

  /*********************************************************
  ****calculate residule from incoming radar measurement****
  **********************************************************/
  VectorXd z = VectorXd(n_z);
  rho = meas_package.raw_measurements_(0);
  phi = meas_package.raw_measurements_(1);
  rho_d = meas_package.raw_measurements_(2);

  z << rho,
       phi,
       rho_d;

  VectorXd z_diff = z - z_pred;
  //normalize angles
  if(z_diff(1) > M_PI)
  {
    z_diff(1) -= 2. * M_PI;
  }
  else if(z_diff(1) < -M_PI)
  {
    z_diff(1) += 2. * M_PI;
  }

  /**************************
  ****calculate NIS laser****
  ***************************/
  NIS_radar_ = z_diff.transpose() * S.inverse() * z_diff;
  cout << "NIS_radar: " << NIS_radar_ << endl;
  
  /***************************************
  ****update state mean and covariance****
  ****************************************/
  x_ += K * z_diff;
  P_ -= K * S * K.transpose();
}
