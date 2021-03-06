/********************************************************************************
 * Copyright 2009 The Robotics Group, The Maersk Mc-Kinney Moller Institute,
 * Faculty of Engineering, University of Southern Denmark
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ********************************************************************************/

#ifndef RWHW_PA10DRIVER_HPP
#define RWHW_PA10DRIVER_HPP

/**
 * @file PA10Driver.hpp
 */

#include "PA10.hpp"

extern "C" {
#include "smsclib.h"
}

#include <rw/math/Q.hpp>
#include <rw/common/macros.hpp>

#include <fstream>
#include <cstring>
namespace rwhw {

    /** @addtogroup pa10 */
    /*@{*/

    /**
     * @brief Implements driver for Mitsubishi PA10
     */
    class PA10Driver: public PA10 {

    public:
        /**
         * @brief Creates driver object
         */
        PA10Driver();

        /**
         * @copydoc PA10::start
         */
        rw::math::Q start(bool& success);

        /**
         * @copydoc PA10::initializeThread
         */
        void initializeThread();

        /**
         * @copydoc PA10::update
         */
        rw::math::Q update(const rw::math::Q& dq);

        /**
         * @copydoc PA10::stop
         */
        void stop();

    private:
        bool receive_pos();

        /**
         * At Power ON the PA10 controller is in adjustment/stop mode and will accept
         * one of the following commands:
         *
         * - 'M' Mechanical zero point measurement
         *
         * - 'T' Control parameter setting
         *
         * - 'T' start - Brake release mode (use 'E' to return to adjustment/stop mode)
         *
         * - 'S' start - Control mode
         * [Command value is received in each control cycle.] (use 'E' to return
         * to adjustment/stop mode, Time-out error will return to
         * adjustment/stop mode)
         *
         * - 'W' Written RAM EEROM table
         *
         * - 'P' Contents EEPROM table is changed
         *
         * - 'R' EEPROM table value is sent
         */

        struct COut{
            // Remember fields are small endian!!!
            struct{
                uint8 mode;
                uint8 torque[2];
                uint8 speed[2];
            }axis[7];
        };

        // Remember all fields are small endian!!!
        struct CIn{
            struct{
                uint8 status[2];
                uint8 angle[4];
                uint8 torque[2];
            }axis[7];
            uint8 status[2];
        };

        void convertVelocities(const rw::math::Q& dq, COut* data);

        void convertPositions(const CIn* data, rw::math::Q& q);

        //! Send PA10 'S' command
        void pa10_send_S();

        void pa10_recv_S();

        //! Send PA10 'E' command
        void pa10_send_E();

        void pa10_recv_E();

        //! Send PA10 'C' command
        void pa10_send_C(const COut* data);

        void pa10_recv_C(CIn* data);
    };

    /**@}*/
} // end namespaces

#endif // end include guard
