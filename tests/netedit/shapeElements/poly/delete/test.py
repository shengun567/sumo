#!/usr/bin/env python
# Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
# Copyright (C) 2009-2019 German Aerospace Center (DLR) and others.
# This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v2.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v20.html
# SPDX-License-Identifier: EPL-2.0

# @file    test.py
# @author  Pablo Alvarez Lopez
# @date    2016-11-25
# @version $Id$

# import common functions for netedit tests
import os
import sys

testRoot = os.path.join(os.environ.get('SUMO_HOME', '.'), 'tests')
neteditTestRoot = os.path.join(
    os.environ.get('TEXTTEST_HOME', testRoot), 'netedit')
sys.path.append(neteditTestRoot)
import neteditTestFunctions as netedit  # noqa

# Open netedit
neteditProcess, referencePosition = netedit.setupAndStart(neteditTestRoot)

# go to shape mode
netedit.shapeMode()

# go to poly mode and select poly
netedit.changeShape("poly")

# create polygon
netedit.createSquaredPoly(referencePosition, 100, 50, 100, True)

# go to delete mode
netedit.deleteMode()

# delete created polygon
netedit.leftClick(referencePosition, 100, 50)

# try to delete second polygon (not possible due isn't fill)
netedit.leftClick(referencePosition, 175, 25)

# delete loaded filled polygon
netedit.leftClick(referencePosition, 275, 25)

# Check undos and redos
netedit.undo(referencePosition, 3)
netedit.redo(referencePosition, 3)

# save shapes
netedit.saveAdditionals()

# save network
netedit.saveNetwork()

# quit netedit
netedit.quit(neteditProcess)
