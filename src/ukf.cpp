#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;


  //Process Noise for Bycicel. Guassian Process: 90% lies in |60 rads/s2|:
  // avg. acc for speed car is 12 m/s2.
  //----------------------------------------------------------------------

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1.75;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 1.25;


  // Measurement Noise, Provided by the manufacturer:
  //-------------------------------------------------
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

  //-------------------------------------------------------------------
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  time_us_= 0;
  is_initialized_ = false;

  n_x_= 5; 				//set state dimension
  n_aug_ = 7; 			// set augmented dimension

  lambda_= 3- n_aug_; 	//define spreading parameter
  weights_ = VectorXd(2* n_aug_ +1); 	//set vector for weights

  x_ = VectorXd(n_x_); // initial state vector
  P_ = MatrixXd(n_x_, n_x_); // initial covariance matrix
  Xsig_pred_= MatrixXd(n_x_, 2 * n_aug_ + 1);		 //create example matrix with predicted sigma points

  //-------------------------------------------------------------------


  // Equations below to set the values was obtained from the form: thanks @fernandodamasio
  // https://discussions.udacity.com/t/increasing-rmse-and-fluctuating-nis-need-help/332736/8

  NIS_laser_ = 0.0;
  NIS_radar_ = 0.0;




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



	/*****************************************************************************
	*  Initialization
	****************************************************************************/

	if (!is_initialized_) {

		/**
		  * Initialize the state ukf_.x_ with the first measurement.
		  * Create the covariance matrix.
		  * convert radar from polar to cartesian coordinates.
		*/

		// first measurement
		cout << "UKF: " << endl;

		//Initialize state

		x_ <<   0.1,   0.1,  0.1,  0.1,  0.1;

		//Initialize covariance matrix

		P_ <<      0.5,    0,       0,    0,    0,
				       0,    1,       0,    0,    0,
				       0,    0,       0.5,    0,    0,
				       0,    0,       0,    0.1,    0,
				       0,    0,       0,     0,    0.1;



		time_us_ = meas_package.timestamp_;

		if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_) {
		  /**
		  Convert radar from polar to cartesian coordinates and initialize state.
		  */

		  float rho_ = meas_package.raw_measurements_[0];
		  float phi_ = meas_package.raw_measurements_[1];

		  float px = rho_ * cos(phi_);
		  float py = rho_ * sin(phi_);


		  x_(0) = px; x_(1) = py;

		}
		else if (meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_) {

			x_(0) = meas_package.raw_measurements_[0];
			x_(1) = meas_package.raw_measurements_[1];

		}


		is_initialized_ = true;

		return;
	}


	// ========= Prediction ===========================================================
	//compute the time elapsed between the current and previous measurements

	float dt = (meas_package.timestamp_ - time_us_) / 1000000.0;	//dt - expressed in seconds
	time_us_ = meas_package.timestamp_;

  if(dt == 0){
    //initialize dt to a small value that will prevent from being zero e.g
    // Thanks to the feedback on P1. :)
    dt = 0.0001;
  }

	Prediction(dt);


	// ========= Update ===========================================================

	if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
	// Radar updates
		UpdateRadar(meas_package);

	} else {
	// lIDAR updates

		UpdateLidar(meas_package);
	}

} // end_of_process measur. fnc.


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


	//-------Create Sgma Points: Ref- [Quiz 1] ----------------------------
	//=====================================================================

	MatrixXd Xsig = MatrixXd(n_x_, 2 * n_x_ + 1); //create sigma point matrix
	MatrixXd A = P_.llt().matrixL();     // //calculate square root of P

	Xsig.col(0)  = x_; 	//set first column of sigma point matrix

	//set remaining sigma points
	for (int i = 0; i < n_x_; i++)
	{
		Xsig.col(i+1)     = x_ + sqrt(lambda_ + n_x_) * A.col(i);
		Xsig.col(i+1 + n_x_) = x_ - sqrt(lambda_ + n_x_) * A.col(i);
	}


	//-------Augment Sigma Points: Ref- [Quiz 2] -----------------------------
	//=====================================================================


	VectorXd x_aug = VectorXd(7);  //create augmented mean vector

	//create augmented mean state
	x_aug.head(5) = x_;
	x_aug(5) = 0;
	x_aug(6) = 0;


	MatrixXd P_aug = MatrixXd(7, 7); //create augmented state covariance

	//create augmented covariance matrix
	P_aug.fill(0.0);
	P_aug.topLeftCorner(5,5) = P_;
	P_aug(5,5) = std_a_* std_a_;
	P_aug(6,6) = std_yawdd_* std_yawdd_;



	MatrixXd L = P_aug.llt().matrixL(); //create square root matrix

	MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1); //create sigma point matrix

	//create augmented sigma points
	Xsig_aug.col(0)  = x_aug;
	for (int i = 0; i< n_aug_; i++)
	{
		Xsig_aug.col(i+1)       = x_aug + sqrt(lambda_ + n_aug_) * L.col(i);
		Xsig_aug.col(i+1 + n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * L.col(i);
	}




	//-------Sigma Points Prediction--Ref: [Quiz 3] -----------------------
	//=====================================================================


	for (int i = 0; i< 2 * n_aug_+1; i++)
	{
		//extract values for better readability
		double p_x = Xsig_aug(0,i);
		double p_y = Xsig_aug(1,i);
		double v = Xsig_aug(2,i);
		double yaw = Xsig_aug(3,i);
		double yawd = Xsig_aug(4,i);
		double nu_a = Xsig_aug(5,i);
		double nu_yawdd = Xsig_aug(6,i);

		double px_p, py_p; //predicted state values

		//avoid division by zero
		if (fabs(yawd) > 0.001) {
			px_p = p_x + v/yawd * ( sin (yaw + yawd * delta_t) - sin(yaw));
			py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd * delta_t) );
		}
		else {
			px_p = p_x + v*delta_t*cos(yaw);
			py_p = p_y + v*delta_t*sin(yaw);
		}

		double v_p = v;
		double yaw_p = yaw + yawd * delta_t;
		double yawd_p = yawd;

		//add noise
		px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
		py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
		v_p = v_p + nu_a*delta_t;

		yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
		yawd_p = yawd_p + nu_yawdd*delta_t;

		//write predicted sigma point into right column
		Xsig_pred_(0,i) = px_p;
		Xsig_pred_(1,i) = py_p;
		Xsig_pred_(2,i) = v_p;
		Xsig_pred_(3,i) = yaw_p;
		Xsig_pred_(4,i) = yawd_p;

	}


	//--------Predict Mean And Covariance --- Ref: [Quiz 4] ----------
	//=====================================================================

	double weight_0 = lambda_/(lambda_+ n_aug_); 	// set weights_
	weights_(0) = weight_0;

	for (int i=1; i<2*n_aug_+1; i++) {  //2n+1 weights

		double weight = 0.5/(n_aug_+lambda_);
		weights_(i) = weight;

	}


	//predicted state mean
	x_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points

		x_ = x_ + weights_(i) * Xsig_pred_.col(i);
	}

	//predicted state covariance matrix
	P_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points

		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;
		//angle normalization
		while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
		while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

		P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
	}

} // End of Prediction func.



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


	//--------Predict Lidar Measurement---------
	//=====================================================================

	//set measurement dimension px, py
	int n_z = 2;

	//create matrix for sigma points in measurement space
	MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

	//transform sigma points into measurement space
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		// extract values for better readibility
		double p_x = Xsig_pred_(0,i);
		double p_y = Xsig_pred_(1,i);

		// measurement model
		Zsig(0,i) = p_x;
		Zsig(1,i) = p_y;

	}

	//mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);
	z_pred.fill(0.0);

	for (int i=0; i < 2*n_aug_ +1; i++) {

		z_pred = z_pred + weights_(i) * Zsig.col(i);
	}

	//measurement covariance matrix S
	MatrixXd S = MatrixXd(n_z,n_z);
	S.fill(0.0);

	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		VectorXd z_diff = Zsig.col(i) - z_pred; //residual

		S = S + weights_(i) * z_diff * z_diff.transpose();
	}

	//add measurement noise covariance matrix

	MatrixXd R = MatrixXd(n_z,n_z);

	R <<    std_laspx_* std_laspx_, 0,
			0, std_laspy_*std_laspy_;


	S = S + R;

	//--------Update Lidar State-------------
	//=====================================================================


	VectorXd z = meas_package.raw_measurements_; // incoming radar measurement

	MatrixXd Tc = MatrixXd(n_x_, n_z); //create matrix for cross correlation Tc

	//calculate cross correlation matrix

	Tc.fill(0.0);

	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points


		VectorXd z_diff = Zsig.col(i) - z_pred; //residual

		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;

		Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
	}


	MatrixXd K = Tc * S.inverse(); //Kalman gain K;

	VectorXd z_diff = z - z_pred; //residual


	//update state mean and covariance matrix
	x_ = x_ + K * z_diff;
	P_ = P_ - K*S*K.transpose();

	// Laser NIC.. Suggestions form the Forum

	NIS_laser_ = z_diff.transpose()* S.inverse() * z_diff;



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


	//--------Predict Radar Measurement--- Ref: [Quiz 5] ----------
	//=====================================================================



	//set measurement dimension, radar can measure r, phi, and r_dot
	int n_z = 3;

	//create matrix for sigma points in measurement space
	MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

	//transform sigma points into measurement space
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		// extract values for better readibility
		double p_x = Xsig_pred_(0,i);
		double p_y = Xsig_pred_(1,i);
		double v  = Xsig_pred_(2,i);
		double yaw = Xsig_pred_(3,i);

		double v1 = cos(yaw)*v;
		double v2 = sin(yaw)*v;

    //Check division by zero
    if (fabs(p_x) < 0.001 and fabs(p_y) < 0.001) {
       p_x = 0.001;
       p_y = 0.001;
    }

		// measurement model
		Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                        //r
		Zsig(1,i) = atan2(p_y,p_x);                                 //phi
		Zsig(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot

	}

	//mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);
	z_pred.fill(0.0);

	for (int i=0; i < 2*n_aug_ +1; i++) {

		z_pred = z_pred + weights_(i) * Zsig.col(i);
	}

	//measurement covariance matrix S
	MatrixXd S = MatrixXd(n_z,n_z);
	S.fill(0.0);

	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		VectorXd z_diff = Zsig.col(i) - z_pred; //residual

		//angle normalization
		while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
		while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

		S = S + weights_(i) * z_diff * z_diff.transpose();
	}

	//add measurement noise covariance matrix

	MatrixXd R = MatrixXd(n_z,n_z);

	R <<    std_radr_*std_radr_,           0,                     0,
			         0,                 std_radphi_*std_radphi_,      0,
			         0,                        0,            std_radrd_*std_radrd_;

	S = S + R;

	//--------Update Radar State--- Ref: [Quiz 6] ----------
	//=====================================================================


	VectorXd z = meas_package.raw_measurements_; // incoming radar measurement

	MatrixXd Tc = MatrixXd(n_x_, n_z); //create matrix for cross correlation Tc

	//calculate cross correlation matrix

	Tc.fill(0.0);

	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points


		VectorXd z_diff = Zsig.col(i) - z_pred; //residual

		//angle normalization
		while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
		while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;

		//angle normalization
		while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
		while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

		Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
	}


	MatrixXd K = Tc * S.inverse(); //Kalman gain K;

	VectorXd z_diff = z - z_pred; //residual

	//angle normalization
	while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
	while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

	//update state mean and covariance matrix
	x_ = x_ + K * z_diff;
	P_ = P_ - K*S*K.transpose();

	// NIS_radar_ Ref. above. From the forum

	NIS_radar_ = z_diff.transpose()* S.inverse() * z_diff;


}
