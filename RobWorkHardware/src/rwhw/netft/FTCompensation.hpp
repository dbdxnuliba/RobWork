#ifndef FTCOMPENSATION_HPP
#define FTCOMPENSATION_HPP

#include "NetFTCommon.hpp"

// Boost
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/circular_buffer.hpp>

// RW
#include <rw/common/Ptr.hpp>
#include <rw/kinematics/State.hpp>
#include <rw/math/Q.hpp>
#include <rw/math/Transform3D.hpp>
#include <rw/math/Vector3D.hpp>
#include <rw/models/Device.hpp>

namespace rwhw {
   /**
    * @brief Compensator for the NetFT device based on a calibration.    * 
    * 
    * This class reads in a current joint configuration,
    * translates this to a F/T tool configuration and outputs
    * a status flag based on a threshold value which tells whether
    * a collision has occurred.
    * 
    * Example usage:
    * 
    * \code
    * FTCompensation ftc(robotdevice, state, calibfile);
    * while(RUNNING) {
    *    ftc.update(ft, q, dq, ddq); // Updates the status flag based on current measurements
    *    if(ftc.inCollision())
    *       ACTION
    * }
    * \endcode
    */
   class FTCompensation {
      public:
	   	typedef rw::common::Ptr<FTCompensation> Ptr;

         /**
          * @brief F/T vector type
          */
//         typedef std::pair<rw::math::Vector3D<>, rw::math::Vector3D<> > Wrench3D;
         
         /**
          * @brief F/T calibration
          */
         struct FTCalib {
            /**
             * Calibration identifier label
             */
            std::string label;
            
            /**
             * Calibrated mass
             */
            double m;
            
            /**
             * Calibrated tool roll
             */
            double a;
            
            /**
             * Calibrated center of gravity (COG)
             */
            rw::math::Vector3D<> r;
            
            /**
             * Calibrated bias
             */
            Wrench3D bias;
         };
         
         /**
          * @brief Constructor for an input device, a workcell state which is copied and a calibration file.
          * 
          * @param dev the robot device
          * @param state the workcell state used for forward kinematics
          * @param calibfile file path for a F/T calibration
          * @param thres the collision threshold
          */
         FTCompensation(rw::models::Device::Ptr dev,
                        rw::kinematics::State state,
                        const std::string& calibfile = "ftcalib.xml",
                        const Wrench3D& thres = Wrench3D(rw::math::Vector3D<>(5, 5, 5), rw::math::Vector3D<>(0.5, 0.5, 0.5)));
         
         virtual ~FTCompensation() {}
         
         /**
          * @brief Updates the collision flag based on current joint configuration and F/T vector.
          * Joint velocity and acceleration are estimated based on input and an internal timer.
          * 
          * The use of this function is in general discouraged since higher-order differentials destabilize.
          * 
          * @param ft current wrench
          * @param q current joint configuration
          */
         inline void update(const Wrench3D& ft, const rw::math::Q& q) {
            // Get elapsed time since last call
            const boost::posix_time::ptime currentT = boost::posix_time::microsec_clock::universal_time();

            // Update clock
            const boost::posix_time::time_duration diffT = currentT - _prevT;
            const double dt = double(diffT.total_microseconds()) * 0.000001;            
            _prevT = currentT;
            
            // Estimate velocity by backward difference
            const rw::math::Q dq = (q - _qP) / dt;
            
            // Update previous position
            _qP = q;
            
            update(ft, q, dq, dt);            
         }
         
         /**
          * @brief Updates the collision flag based on current joint configuration, velocity and F/T vector.
          * Joint velocity and acceleration are estimated based on input and an internal timer.
          * 
          * @param ft current wrench
          * @param q current joint configuration
          * @param dq current joint velocity
          */
         inline void update(const Wrench3D& ft, const rw::math::Q& q, const rw::math::Q& dq) {
            // Get elapsed time since last call
            const boost::posix_time::ptime currentT = boost::posix_time::microsec_clock::universal_time();

            // Update clock
            const boost::posix_time::time_duration diffT = currentT - _prevT;
            const double dt = double(diffT.total_microseconds()) * 0.000001;            
            _prevT = currentT;
            
            update(ft, q, dq, dt);
         }
         
         /**
          * @brief Updates the collision flag based on current joint configuration, velocity and F/T vector.
          * Joint velocity and acceleration are estimated based on input and an internal timer.
          * 
          * @param ft current wrench
          * @param q current joint configuration
          * @param dq current joint velocity
          * @param dt time between samples
          */
         inline void update(const Wrench3D& ft,
                            const rw::math::Q& q,
                            const rw::math::Q& dq,
                            double dt) {
            // Estimate acceleration by backward difference
            const rw::math::Q ddq = (dq - _dqP) / dt;
            
            // Update previous velocity
            _dqP = dq;
            
            update(ft, q, dq, ddq, dt);
         }
         
         /**
          * @brief Updates the current wrench and the  collision flag
          * based on current joint configuration, velocity, acceleration and F/T vector.
          * 
          * @param ft current wrench
          * @param q current joint configuration
          * @param dq current joint velocity
          * @param ddq current joint acceleration
          */
         void update(const Wrench3D& ft,
                     const rw::math::Q& q,
                     const rw::math::Q& dq,
                     const rw::math::Q& ddq,
                     double dt);
         
         /**
          * Get current wrench, which is compensated internally
          * 
          * @return current wrench, which is compensated internally
          */
         inline const Wrench3D& getFT() const { return _ft; }
         
         /**
          * @brief Collision status
          * 
          * @return collision flag
          */
         inline double inCollision() const { return _collision; }
         
         /**
          * @brief Set calibration
          * 
          * @param calib calibration
          */
         inline void setCalib(const FTCalib& calib) { _calib = calib; }
         
         /**
          * Utility function: load a calibration from an XML file
          * 
          * @param filename the file to load from
          * @param calib the calibration object to write to
          * @param eTft the calibrated end-effector to F/T transformation
          * @return failure is loading fails
          */
         static bool LoadCalib(const std::string& filename, FTCalib& calib, rw::math::Transform3D<>& eTft);
         
         /**
          * Update the F/T sensor bias based on a set of recent measurements, which can be necessary under e.g.
          * temperature drift.
          * 
          * It is the intention that this function be used from a set of recent outputs using \code getFT() \endcode,
          * which should ideally be zero. Therefore, it is advised to keep the sensor steady while retrieving
          * a set of e.g. 20 measurements as input for this function.
          * 
          * @param biases a set of recent (compensated measurements), retrieved by \code getFT() \endcode
          */
         inline void updateBias(const std::vector<Wrench3D>& biases) {
            // Calculate the mean of the input vector 
            Wrench3D meanBias;
            for(std::vector<Wrench3D>::const_iterator it = biases.begin(); it != biases.end(); ++it) {
               meanBias.first += it->first;
               meanBias.second += it->second;
            }
            const double n = static_cast<double>(biases.size());
            meanBias.first /= n;
            meanBias.second /= n;
            
            // Add to bias            
            _calib.bias.first += meanBias.first;
            _calib.bias.second += meanBias.second;
         }
         
         /**
          * Remove old bias vector and replace with a new one
          *  
          * @param bias the new bias
          */
         inline void resetBias(const Wrench3D& bias) {
            _calib.bias = bias;
         }
         
      private:
         /**
          * Default constructor
          */
         FTCompensation() {}

         /**
          * Calculate F/T tool forward kinematics (FK)
          * 
          * @param q current joint configuration
          * @param eTft robot tool to F/T tool transformation
          * @return robot base to F/T tool FK
          */
         inline rw::math::Transform3D<> fk(const rw::math::Q& q, const rw::math::Transform3D<>& eTft) {
            rw::math::Transform3D<> result;
            
            // Update base-relative tool FK
            _dev->setQ(q, _state);
            result = _dev->baseTend(_state);
            
            // Update F/T tool FK
            result.R() = result.R() * eTft.R();
            
            return result;
         }
         
         /**
          * Estimate current acceleration of the COG
          * 
          * @return estimated COG acceleration
          */
         rw::math::Vector3D<> ddp(double dt);
         
         /**
          * Bias F/T vector
          */
         void unbias();
         
         /**
          * Compensate F/T measurement for gravity
          */
         void ungravitate();
         
         /**
          * Compensate F/T measurement for acceleration
          * 
          * @param a acceleration of the COG
          */
         void decelerate(const rw::math::Vector3D<>& a);
         
         /**
          * Device pointer
          */
         rw::models::Device::Ptr _dev;
         
         /**
          * Workcell state copy
          */
         rw::kinematics::State _state;
         
         /**
          * Copy of current wrench, which is compensated internally
          */
         Wrench3D _ft;
         
         /**
          * Robot tool to F/T tool transformation
          */
         rw::math::Transform3D<> _eTft;
         
         /**
          * Current forward kinematics of the F/T tool based on current joint position
          */
         rw::math::Transform3D<> _bTft;
         
         /**
          * Threshold value, must be positive
          */
         Wrench3D _thres;
         
         /**
          * Calibration
          */
         FTCalib _calib;
         
         /**
          * Timer used for estimating joint velocity/acceleration if necessary
          */
         boost::posix_time::ptime _prevT;
         
         /**
          * Previous joint position/velocity for estimating acceleration
          */
         rw::math::Q _qP, _dqP;
         
         /**
          * Number of samples (filter order + 1) used for estimating acceleration
          */
         unsigned int _horizon;
         
         /**
          * Previous tool translations raw/filtered for estimating current acceleration
          */
         boost::circular_buffer<rw::math::Vector3D<> > _ts, _tlpfs;
         
         /**
          * Filter used for estimating tool acceleration
          */
         rwhw::IIRFilterB5W20 _filter;
         
         /**
          * Collision flag, true on threshold violation
          */
         bool _collision;
   };
}

#endif
