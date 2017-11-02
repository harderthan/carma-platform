/*
 * Copyright (C) 2017 LEIDOS.
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

package gov.dot.fhwa.saxton.carma.guidance.maneuvers;

import gov.dot.fhwa.saxton.carma.guidance.IGuidanceCommands;

/**
 * Defines an interface to all Maneuver objects.
 */
public interface IManeuver {

    /**
     * Plans the maneuver to and makes it ready for execution
     *
     * @param inputs - the object that provides necessary input data about the route
     * @param commands - the object that will take output commands
     * @param startDist - distance from beginning of route at which this maneuver is to begin, m
     * @param targetSpeed - the speed at which this maneuver is to complete, m/s
     * @throws IllegalStateException if required target quantity is not defined prior to this call
     */
    void plan(IManeuverInputs inputs, IGuidanceCommands commands, double startDist) throws IllegalStateException, UnsupportedOperationException;

    /**
     * Plans the maneuver to a target end distance and makes it ready for execution
     *
     * @param inputs - the object that provides necessary input data about the route
     * @param commands - the object that will take output commands
     * @param startDist - distance from beginning of route at which this maneuver is to begin, m
     * @param endDist - the distance from the beginning of route at which this maneuver is to end, m
     * @throws IllegalStateException if required target quantity is not defined prior to this call
     */
    void planToTargetDistance(IManeuverInputs inputs, IGuidanceCommands commands, double startDist, double endDist) throws IllegalStateException;


    /**
     * Executes a single time step of the maneuver that has already been planned, by calculating the
     * instantaneous commands then passing those commands to the vehicle's controller driver.  There is no
     * assumption made about the duration or uniformity of time steps.
     *
     * ASSUMES that the plan method has already run to completion - there is no check for this condition!
     *
     * @return true if the maneuver has completed; false if it is still in progress
     * @throws IllegalStateException if called when vehicle's position along route is not between the
     * maneuver's start and end distances
     */
    boolean executeTimeStep() throws IllegalStateException;


    /**
     * Stores the beginning and target speed of the maneuver, to be used for longitudinal maneuvers only.
     * Since maneuvers will generally be chained together during planning, this is the only way that a maneuver
     * can know what speed the vehicle will have after completing its predecessor maneuver.
     * @param startSpeed - the expected speed at the beginning of the maneuver, m/s
     * @param targetSpeed - target speed at end of maneuver, m/s
     * @throws UnsupportedOperationException if called on a lateral maneuver object
     */
    @Deprecated
    void setSpeeds(double startSpeed, double targetSpeed) throws UnsupportedOperationException;


    /**
     * Returns the specified starting speed for the maneuver.  To be used for longitudinal maneuvers only.
     * @return m/s
     * @throws UnsupportedOperationException if called on a lateral maneuver object
     */
    @Deprecated
    double getStartSpeed() throws UnsupportedOperationException;


    /**
     * Returns the specified target speed for the end of the maneuver.  To be used for longitudinal maneuvers only.
     * @return m/s
     * @throws UnsupportedOperationException if called on a lateral maneuver object
     */
    double getTargetSpeed() throws UnsupportedOperationException;


    /**
     * Stores the target lane ID, to be used for lateral maneuvers only.
     * @param targetLane - target lane number at end of maneuver
     * @throws UnsupportedOperationException if called on a longitudinal maneuver object
     */
    void setTargetLane(int targetLane) throws UnsupportedOperationException;


    /**
     * Retrieves the specified start distance of the maneuver.
     * @return distance from beginning of the route at which the maneuver begins, m
     */
    double getStartDistance();


    /**
     * Retrieves the calculated end distance of the maneuver.
     * @return distance from beginning of the route at which the maneuver is to complete, m
     */
    double getEndDistance();
}