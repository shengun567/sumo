/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2020 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    GNEStoppingPlace.cpp
/// @author  Pablo Alvarez Lopez
/// @date    Dec 2015
///
// A abstract class to define common parameters of lane area in which vehicles can halt (GNE version)
/****************************************************************************/

// ===========================================================================
// included modules
// ===========================================================================

#include <netedit/GNENet.h>
#include <netedit/GNEUndoList.h>
#include <netedit/GNEViewNet.h>
#include <netedit/changes/GNEChange_Attribute.h>
#include <netedit/elements/demand/GNEDemandElement.h>
#include <netedit/elements/network/GNEEdge.h>
#include <netedit/elements/network/GNELane.h>
#include <utils/options/OptionsCont.h>
#include <utils/vehicle/SUMORouteHandler.h>

#include "GNEStoppingPlace.h"

// ===========================================================================
// static members
// ===========================================================================

const double GNEStoppingPlace::myCircleWidth = 1.1;
const double GNEStoppingPlace::myCircleWidthSquared = 1.21;
const double GNEStoppingPlace::myCircleInWidth = 0.9;
const double GNEStoppingPlace::myCircleInText = 1.6;

// ===========================================================================
// member method definitions
// ===========================================================================

GNEStoppingPlace::GNEStoppingPlace(const std::string& id, GNEViewNet* viewNet, GUIGlObjectType type, SumoXMLTag tag, GNELane* lane, double startPos, double endPos,
                                   int parametersSet, const std::string& name, bool friendlyPosition, bool blockMovement) :
    GNEAdditional(id, viewNet, type, tag, name, blockMovement, 
        {}, {lane}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}),
    myStartPosition(startPos),
    myEndPosition(endPos),
    myParametersSet(parametersSet),
    myFriendlyPosition(friendlyPosition) {
}


GNEStoppingPlace::~GNEStoppingPlace() {}


bool
GNEStoppingPlace::isAdditionalValid() const {
    // with friendly position enabled position are "always fixed"
    if (myFriendlyPosition) {
        return true;
    } else {
        // obtain lane length
        double laneLength = getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength() * getParentLanes().front()->getLengthGeometryFactor();
        // calculate start and end positions
        double startPos = (myParametersSet & STOPPINGPLACE_STARTPOS_SET) ? myStartPosition : 0;
        double endPos = (myParametersSet & STOPPINGPLACE_ENDPOS_SET) ? myEndPosition : laneLength;
        // check if position has to be fixed
        if (startPos < 0) {
            startPos += laneLength;
        }
        if (endPos < 0) {
            endPos += laneLength;
        }
        // check values
        if (myParametersSet == 0) {
            return true;
        } else if ((myParametersSet & STOPPINGPLACE_STARTPOS_SET) == 0) {
            return (endPos <= getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength());
        } else if ((myParametersSet & STOPPINGPLACE_ENDPOS_SET) == 0) {
            return (startPos >= 0);
        } else {
            return ((startPos >= 0) && (endPos <= getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength()) && ((endPos - startPos) >= POSITION_EPS));
        }
    }
}


std::string
GNEStoppingPlace::getAdditionalProblem() const {
    // calculate start and end positions
    double startPos = (myParametersSet & STOPPINGPLACE_STARTPOS_SET) ? myStartPosition : 0;
    double endPos = (myParametersSet & STOPPINGPLACE_ENDPOS_SET) ? myEndPosition : getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength();
    // obtain lane length
    double laneLength = getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength();
    // check if position has to be fixed
    if (startPos < 0) {
        startPos += laneLength;
    }
    if (endPos < 0) {
        endPos += laneLength;
    }
    // declare variables
    std::string errorStart, separator, errorEnd;
    // check positions over lane
    if (startPos < 0) {
        errorStart = (toString(SUMO_ATTR_STARTPOS) + " < 0");
    } else if (startPos > getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength()) {
        errorStart = (toString(SUMO_ATTR_STARTPOS) + " > lanes's length");
    }
    if (endPos < 0) {
        errorEnd = (toString(SUMO_ATTR_ENDPOS) + " < 0");
    } else if (endPos > getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength()) {
        errorEnd = (toString(SUMO_ATTR_ENDPOS) + " > lanes's length");
    }
    // check separator
    if ((errorStart.size() > 0) && (errorEnd.size() > 0)) {
        separator = " and ";
    }
    return errorStart + separator + errorEnd;
}


void
GNEStoppingPlace::fixAdditionalProblem() {
    // declare new start and end position
    double newStartPos = myStartPosition;
    double newEndPos = myEndPosition;
    // fix start and end positions using fixStoppingPlacePosition
    SUMORouteHandler::checkStopPos(newStartPos, newEndPos, getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength(), POSITION_EPS, true);
    // set new start and end positions
    setAttribute(SUMO_ATTR_STARTPOS, toString(newStartPos), myViewNet->getUndoList());
    setAttribute(SUMO_ATTR_ENDPOS, toString(newEndPos), myViewNet->getUndoList());
}


Position
GNEStoppingPlace::getPositionInView() const {
    // calculate start and end positions as absolute values
    double startPos = fabs((myParametersSet & STOPPINGPLACE_STARTPOS_SET) ? myStartPosition : 0);
    double endPos = fabs((myParametersSet & STOPPINGPLACE_ENDPOS_SET) ? myEndPosition : getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength());
    // obtain position in view depending if both positions are defined
    if (myParametersSet == 0) {
        return getParentLanes().front()->getLaneShape().positionAtOffset(getParentLanes().front()->getLaneShape().length() / 2);
    } else if ((myParametersSet & STOPPINGPLACE_STARTPOS_SET) == 0) {
        return getParentLanes().front()->getLaneShape().positionAtOffset(endPos);
    } else if ((myParametersSet & STOPPINGPLACE_ENDPOS_SET) == 0) {
        return getParentLanes().front()->getLaneShape().positionAtOffset(startPos);
    } else {
        return getParentLanes().front()->getLaneShape().positionAtOffset((startPos + endPos) / 2.0);
    }
}


void
GNEStoppingPlace::splitEdgeGeometry(const double splitPosition, const GNENetworkElement* originalElement, const GNENetworkElement* newElement, GNEUndoList* undoList) {
    // first check tat both network elements are lanes and originalElement correspond to stoppingPlace lane
    if ((originalElement->getTagProperty().getTag() == SUMO_TAG_LANE) &&
            (originalElement->getTagProperty().getTag() == SUMO_TAG_LANE) &&
            (getParentLanes().front() == originalElement)) {
        // check if we have to change additional lane depending of split position
        if ((myParametersSet & STOPPINGPLACE_STARTPOS_SET) && (myParametersSet & STOPPINGPLACE_ENDPOS_SET)) {
            // calculate middle position
            const double middlePosition = ((myEndPosition - myStartPosition) / 2.0) + myStartPosition;
            //  four cases:
            if (splitPosition < myStartPosition) {
                // change lane
                setAttribute(SUMO_ATTR_LANE, newElement->getID(), undoList);
                // now adjust start and end position
                setAttribute(SUMO_ATTR_STARTPOS, toString(myStartPosition - splitPosition), undoList);
                setAttribute(SUMO_ATTR_ENDPOS, toString(myEndPosition - splitPosition), undoList);
            } else if ((splitPosition > myStartPosition) && (splitPosition < middlePosition)) {
                // change lane
                setAttribute(SUMO_ATTR_LANE, newElement->getID(), undoList);
                // now adjust start and end position
                setAttribute(SUMO_ATTR_STARTPOS, "0", undoList);
                setAttribute(SUMO_ATTR_ENDPOS, toString(myEndPosition - splitPosition), undoList);
            } else if ((splitPosition > middlePosition) && (splitPosition < myEndPosition)) {
                // only adjust end position
                setAttribute(SUMO_ATTR_ENDPOS, toString(splitPosition), undoList);
            } else if ((splitPosition > myEndPosition)) {
                // nothing to do
            }
        } else if ((myParametersSet & STOPPINGPLACE_STARTPOS_SET) && (splitPosition < myStartPosition)) {
            // change lane
            setAttribute(SUMO_ATTR_LANE, newElement->getID(), undoList);
            // now adjust start position
            setAttribute(SUMO_ATTR_STARTPOS, toString(myStartPosition - splitPosition), undoList);
        } else if ((myParametersSet & STOPPINGPLACE_ENDPOS_SET) && (splitPosition < myEndPosition)) {
            // change lane
            setAttribute(SUMO_ATTR_LANE, newElement->getID(), undoList);
            // now adjust end position
            setAttribute(SUMO_ATTR_ENDPOS, toString(myEndPosition - splitPosition), undoList);
        }
    }
}


void
GNEStoppingPlace::moveGeometry(const Position& offset) {
    // only move if at leats start or end positions is defined
    if (myParametersSet > 0) {
        // Calculate new position using old position
        Position newPosition = myMove.originalViewPosition;
        newPosition.add(offset);
        // filtern position using snap to active grid
        newPosition = myViewNet->snapToActiveGrid(newPosition);
        double offsetLane = getParentLanes().front()->getLaneShape().nearest_offset_to_point2D(newPosition, false) - getParentLanes().front()->getLaneShape().nearest_offset_to_point2D(myMove.originalViewPosition, false);
        // check if both position has to be moved
        if ((myParametersSet & STOPPINGPLACE_STARTPOS_SET) && (myParametersSet & STOPPINGPLACE_ENDPOS_SET)) {
            // calculate stoppingPlace length and lane length (After apply geometry factor)
            double stoppingPlaceLength = fabs(parse<double>(myMove.secondOriginalPosition) - parse<double>(myMove.firstOriginalLanePosition));
            double laneLengt = getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength() * getParentLanes().front()->getLengthGeometryFactor();
            // avoid changing stopping place's length
            if ((parse<double>(myMove.firstOriginalLanePosition) + offsetLane) < 0) {
                myStartPosition = 0;
                myEndPosition = stoppingPlaceLength;
            } else if ((parse<double>(myMove.secondOriginalPosition) + offsetLane) > laneLengt) {
                myStartPosition = laneLengt - stoppingPlaceLength;
                myEndPosition = laneLengt;
            } else {
                myStartPosition = parse<double>(myMove.firstOriginalLanePosition) + offsetLane;
                myEndPosition = parse<double>(myMove.secondOriginalPosition) + offsetLane;
            }
        } else {
            // check if start position must be moved
            if (myParametersSet & STOPPINGPLACE_STARTPOS_SET) {
                myStartPosition = parse<double>(myMove.firstOriginalLanePosition) + offsetLane;
            }
            // check if start position must be moved
            if (myParametersSet & STOPPINGPLACE_ENDPOS_SET) {
                myEndPosition = parse<double>(myMove.secondOriginalPosition) + offsetLane;
            }
        }
        // Update geometry
        updateGeometry();
    }
}


void
GNEStoppingPlace::commitGeometryMoving(GNEUndoList* undoList) {
    // only commit geometry moving if at leats start or end positions is defined
    if (myParametersSet > 0) {
        undoList->p_begin("position of " + getTagStr());
        if (myParametersSet & STOPPINGPLACE_STARTPOS_SET) {
            undoList->p_add(new GNEChange_Attribute(this, myViewNet->getNet(), SUMO_ATTR_STARTPOS, toString(myStartPosition), true, myMove.firstOriginalLanePosition));
        }
        if (myParametersSet & STOPPINGPLACE_ENDPOS_SET) {
            undoList->p_add(new GNEChange_Attribute(this, myViewNet->getNet(), SUMO_ATTR_ENDPOS, toString(myEndPosition), true, myMove.secondOriginalPosition));
        }
        undoList->p_end();
    }
}


double
GNEStoppingPlace::getStartPosition() const {
    if (myParametersSet & STOPPINGPLACE_STARTPOS_SET) {
        return myStartPosition;
    } else {
        return 0;
    }
}


double
GNEStoppingPlace::getEndPosition() const {
    if (myParametersSet & STOPPINGPLACE_ENDPOS_SET) {
        return myEndPosition;
    } else {
        return getParentLanes().front()->getLaneShapeLength();
    }
}


std::string
GNEStoppingPlace::getParentName() const {
    return getParentLanes().front()->getMicrosimID();
}


void
GNEStoppingPlace::setStoppingPlaceGeometry(double movingToSide) {
    // Get value of option "lefthand"
    const double offsetSign = OptionsCont::getOptions().getBool("lefthand") ? -1 : 1;

    // obtain laneShape
    PositionVector laneShape = getParentLanes().front()->getLaneShape();

    // Move shape to side
    laneShape.move2side(movingToSide * offsetSign);

    // Cut shape using as delimitators fixed start position and fixed end position
    myAdditionalGeometry.updateGeometry(laneShape, getStartGeometryPositionOverLane(), getEndGeometryPositionOverLane());
}


double
GNEStoppingPlace::getStartGeometryPositionOverLane() const {
    if (myParametersSet & STOPPINGPLACE_STARTPOS_SET) {
        double fixedPos = myStartPosition;
        const double len = getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength();
        if (fixedPos < 0) {
            fixedPos += len;
        }
        return fixedPos * getParentLanes().front()->getLengthGeometryFactor();
    } else {
        return 0;
    }
}


double
GNEStoppingPlace::getEndGeometryPositionOverLane() const {
    if (myParametersSet & STOPPINGPLACE_ENDPOS_SET) {
        double fixedPos = myEndPosition;
        const double len = getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength();
        if (fixedPos < 0) {
            fixedPos += len;
        }
        return fixedPos * getParentLanes().front()->getLengthGeometryFactor();
    } else {
        return getParentLanes().front()->getParentEdge()->getNBEdge()->getFinalLength();
    }
}


double
GNEStoppingPlace::getAttributeDouble(SumoXMLAttr key) const {
    switch (key) {
        case SUMO_ATTR_STARTPOS:
            if (myParametersSet & STOPPINGPLACE_STARTPOS_SET) {
                return myStartPosition;
            } else {
                return -1;
            }
        case SUMO_ATTR_ENDPOS:
            if (myParametersSet & STOPPINGPLACE_ENDPOS_SET) {
                return myEndPosition;
            } else {
                return -1;
            }
        default:
            throw InvalidArgument(getTagStr() + " doesn't have a double attribute of type '" + toString(key) + "'");
    }
}


bool
GNEStoppingPlace::isAttributeEnabled(SumoXMLAttr /*key*/) const {
    // all stopping place attributes are always enabled
    return true;
}


std::string
GNEStoppingPlace::getPopUpID() const {
    return getTagStr() + ": " + getID();
}


std::string
GNEStoppingPlace::getHierarchyName() const {
    return getTagStr();
}

/****************************************************************************/
