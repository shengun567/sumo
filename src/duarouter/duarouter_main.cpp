/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2019 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    duarouter_main.cpp
/// @author  Daniel Krajzewicz
/// @author  Jakob Erdmann
/// @author  Michael Behrisch
/// @date    Thu, 06 Jun 2002
///
// Main for DUAROUTER
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#include <config.h>

#ifdef HAVE_VERSION_H
#include <version.h>
#endif

#include <iostream>
#include <string>
#include <limits.h>
#include <ctime>
#include <memory>
#include <xercesc/sax/SAXException.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <utils/common/StringUtils.h>
#include <utils/common/MsgHandler.h>
#include <utils/common/UtilExceptions.h>
#include <utils/common/SystemFrame.h>
#include <utils/common/RandHelper.h>
#include <utils/common/ToString.h>
#include <utils/iodevices/OutputDevice.h>
#include <utils/options/Option.h>
#include <utils/options/OptionsCont.h>
#include <utils/options/OptionsIO.h>
#include <utils/router/DijkstraRouter.h>
#include <utils/router/AStarRouter.h>
#include <utils/router/CHRouter.h>
#include <utils/router/CHRouterWrapper.h>
#include <utils/xml/XMLSubSys.h>
#include <router/ROFrame.h>
#include <router/ROLoader.h>
#include <router/RONet.h>
#include <router/ROEdge.h>
#include "RODUAEdgeBuilder.h"
#include "RODUAFrame.h"


// ===========================================================================
// functions
// ===========================================================================
/* -------------------------------------------------------------------------
 * data processing methods
 * ----------------------------------------------------------------------- */
/**
 * loads the net
 * The net is in this meaning made up by the net itself and the dynamic
 * weights which may be supplied in a separate file
 */
void
initNet(RONet& net, ROLoader& loader, OptionsCont& oc) {
    // load the net
    RODUAEdgeBuilder builder;
    ROEdge::setGlobalOptions(oc.getBool("weights.interpolate"));
    loader.loadNet(net, builder);
    // load the weights when wished/available
    if (oc.isSet("weight-files")) {
        loader.loadWeights(net, "weight-files", oc.getString("weight-attribute"), false, oc.getBool("weights.expand"));
    }
    if (oc.isSet("lane-weight-files")) {
        loader.loadWeights(net, "lane-weight-files", oc.getString("weight-attribute"), true, oc.getBool("weights.expand"));
    }
}



/**
 * Computes the routes saving them
 */
void
computeRoutes(RONet& net, ROLoader& loader, OptionsCont& oc) {
    // initialise the loader
    loader.openRoutes(net);
    // build the router
    auto ttFunction = gWeightsRandomFactor > 1 ? &ROEdge::getTravelTimeStaticRandomized : &ROEdge::getTravelTimeStatic;
    SUMOAbstractRouter<ROEdge, ROVehicle>* router;
    const std::string measure = oc.getString("weight-attribute");
    const std::string routingAlgorithm = oc.getString("routing-algorithm");
    const SUMOTime begin = string2time(oc.getString("begin"));
    const SUMOTime end = string2time(oc.getString("end"));
    if (measure == "traveltime") {
        if (routingAlgorithm == "dijkstra") {
            router = new DijkstraRouter<ROEdge, ROVehicle>(ROEdge::getAllEdges(), oc.getBool("ignore-errors"), ttFunction, nullptr, false, nullptr, net.hasPermissions(), oc.isSet("restriction-params"));
        } else if (routingAlgorithm == "astar") {
            typedef AStarRouter<ROEdge, ROVehicle> AStar;
            std::shared_ptr<const AStar::LookupTable> lookup;
            if (oc.isSet("astar.all-distances")) {
                lookup = std::make_shared<const AStar::FLT>(oc.getString("astar.all-distances"), (int)ROEdge::getAllEdges().size());
            } else if (oc.isSet("astar.landmark-distances")) {
                CHRouterWrapper<ROEdge, ROVehicle> chrouter(
                    ROEdge::getAllEdges(), true, &ROEdge::getTravelTimeStatic,
                    begin, end, std::numeric_limits<int>::max(), 1);
                ROVehicle defaultVehicle(SUMOVehicleParameter(), nullptr, net.getVehicleTypeSecure(DEFAULT_VTYPE_ID), &net);
                lookup = std::make_shared<const AStar::LMLT>(oc.getString("astar.landmark-distances"), ROEdge::getAllEdges(), &chrouter, &defaultVehicle,
                         oc.isSet("astar.save-landmark-distances") ? oc.getString("astar.save-landmark-distances") : "", oc.getInt("routing-threads"));
            }
            router = new AStar(ROEdge::getAllEdges(), oc.getBool("ignore-errors"), ttFunction, lookup, net.hasPermissions(), oc.isSet("restriction-params"));
        } else if (routingAlgorithm == "CH") {
            const SUMOTime weightPeriod = (oc.isSet("weight-files") ?
                                           string2time(oc.getString("weight-period")) :
                                           SUMOTime_MAX);
            router = new CHRouter<ROEdge, ROVehicle>(
                ROEdge::getAllEdges(), oc.getBool("ignore-errors"), &ROEdge::getTravelTimeStatic, SVC_IGNORING, weightPeriod, net.hasPermissions(), oc.isSet("restriction-params"));
        } else if (routingAlgorithm == "CHWrapper") {
            const SUMOTime weightPeriod = (oc.isSet("weight-files") ?
                                           string2time(oc.getString("weight-period")) :
                                           SUMOTime_MAX);
            router = new CHRouterWrapper<ROEdge, ROVehicle>(
                ROEdge::getAllEdges(), oc.getBool("ignore-errors"), &ROEdge::getTravelTimeStatic,
                begin, end, weightPeriod, oc.getInt("routing-threads"));
        } else {
            throw ProcessError("Unknown routing Algorithm '" + routingAlgorithm + "'!");
        }
    } else {
        DijkstraRouter<ROEdge, ROVehicle>::Operation op;
        if (measure == "CO") {
            op = &ROEdge::getEmissionEffort<PollutantsInterface::CO>;
        } else if (measure == "CO2") {
            op = &ROEdge::getEmissionEffort<PollutantsInterface::CO2>;
        } else if (measure == "PMx") {
            op = &ROEdge::getEmissionEffort<PollutantsInterface::PM_X>;
        } else if (measure == "HC") {
            op = &ROEdge::getEmissionEffort<PollutantsInterface::HC>;
        } else if (measure == "NOx") {
            op = &ROEdge::getEmissionEffort<PollutantsInterface::NO_X>;
        } else if (measure == "fuel") {
            op = &ROEdge::getEmissionEffort<PollutantsInterface::FUEL>;
        } else if (measure == "electricity") {
            op = &ROEdge::getEmissionEffort<PollutantsInterface::ELEC>;
        } else if (measure == "noise") {
            op = &ROEdge::getNoiseEffort;
        } else {
            throw ProcessError("Unknown measure (weight attribute '" + measure + "')!");
        }
        router = new DijkstraRouter<ROEdge, ROVehicle>(
            ROEdge::getAllEdges(), oc.getBool("ignore-errors"), op, ttFunction, false, nullptr, net.hasPermissions(), oc.isSet("restriction-params"));
    }
    int carWalk = 0;
    for (const std::string& opt : oc.getStringVector("persontrip.transfer.car-walk")) {
        if (opt == "parkingAreas") {
            carWalk |= ROIntermodalRouter::Network::PARKING_AREAS;
        } else if (opt == "ptStops") {
            carWalk |= ROIntermodalRouter::Network::PT_STOPS;
        } else if (opt == "allJunctions") {
            carWalk |= ROIntermodalRouter::Network::ALL_JUNCTIONS;
        } else if (opt == "taxi") {
            carWalk |= ROIntermodalRouter::Network::ALL_JUNCTIONS_TAXI;
        }
    }
    RORouterProvider provider(router, new PedestrianRouter<ROEdge, ROLane, RONode, ROVehicle>(),
                              new ROIntermodalRouter(RONet::adaptIntermodalRouter, carWalk, routingAlgorithm));
    // process route definitions
    try {
        net.openOutput(oc);
        loader.processRoutes(begin, end, string2time(oc.getString("route-steps")), net, provider);
        net.writeIntermodal(oc, provider.getIntermodalRouter());
        // end the processing
        net.cleanup();
    } catch (ProcessError&) {
        net.cleanup();
        throw;
    }
}


/* -------------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int
main(int argc, char** argv) {
    OptionsCont& oc = OptionsCont::getOptions();
    // give some application descriptions
    oc.setApplicationDescription("Shortest path router and DUE computer for the microscopic, multi-modal traffic simulation SUMO.");
    oc.setApplicationName("duarouter", "Eclipse SUMO duarouter Version " VERSION_STRING);
    int ret = 0;
    RONet* net = nullptr;
    try {
        XMLSubSys::init();
        RODUAFrame::fillOptions();
        OptionsIO::setArgs(argc, argv);
        OptionsIO::getOptions();
        if (oc.processMetaOptions(argc < 2)) {
            SystemFrame::close();
            return 0;
        }
        XMLSubSys::setValidation(oc.getString("xml-validation"), oc.getString("xml-validation.net"));
        MsgHandler::initOutputOptions();
        if (!(RODUAFrame::checkOptions() && SystemFrame::checkOptions())) {
            throw ProcessError();
        }
        RandHelper::initRandGlobal();
        // load data
        ROLoader loader(oc, false, !oc.getBool("no-step-log"));
        net = new RONet();
        initNet(*net, loader, oc);
        // build routes
        try {
            computeRoutes(*net, loader, oc);
        } catch (XERCES_CPP_NAMESPACE::SAXParseException& e) {
            WRITE_ERROR(toString(e.getLineNumber()));
            ret = 1;
        } catch (XERCES_CPP_NAMESPACE::SAXException& e) {
            WRITE_ERROR(StringUtils::transcode(e.getMessage()));
            ret = 1;
        }
        if (MsgHandler::getErrorInstance()->wasInformed() || ret != 0) {
            throw ProcessError();
        }
    } catch (const ProcessError& e) {
        if (std::string(e.what()) != std::string("Process Error") && std::string(e.what()) != std::string("")) {
            WRITE_ERROR(e.what());
        }
        MsgHandler::getErrorInstance()->inform("Quitting (on error).", false);
        ret = 1;
#ifndef _DEBUG
    } catch (const std::exception& e) {
        if (std::string(e.what()) != std::string("")) {
            WRITE_ERROR(e.what());
        }
        MsgHandler::getErrorInstance()->inform("Quitting (on error).", false);
        ret = 1;
    } catch (...) {
        MsgHandler::getErrorInstance()->inform("Quitting (on unknown error).", false);
        ret = 1;
#endif
    }
    delete net;
    SystemFrame::close();
    if (ret == 0) {
        std::cout << "Success." << std::endl;
    }
    return ret;
}



/****************************************************************************/

