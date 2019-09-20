#pragma once

/*
 * Copyright (C) 2019 LEIDOS.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#include <cav_msgs/DriverStatus.h>
#include "entry_manager.h"

namespace health_monitor
{
    class DriverManager
    {
        public:
            
            /*!
             * \brief Default constructor for DriverManager
             */
            DriverManager();
            
            /*!
             * \brief Constructor for DriverManager takes in crtitical driver names and driver timeout
             */
            DriverManager(std::vector<std::string> critical_driver_names, const long driver_timeout);

            /*!
             * \brief Update driver status
             */
            void update_driver_status(const cav_msgs::DriverStatusConstPtr& msg, long current_time);

            /*!
             * \brief Check if all critical drivers are operational
             */
            bool are_critical_drivers_operational(long current_time);


        private:

            EntryManager em_;
            // timeout for critical driver timeout
            long driver_timeout_;
            // number of critical drivers
            int critical_driver_number_;

    };
}