#*************************************************************************
#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# Copyright 2000, 2011 Oracle and/or its affiliates.
#
# OpenOffice.org - a multi-platform office productivity suite
#
# This file is part of OpenOffice.org.
#
# OpenOffice.org is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 3
# only, as published by the Free Software Foundation.
#
# OpenOffice.org is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License version 3 for more details
# (a copy is included in the LICENSE file that accompanied this code).
#
# You should have received a copy of the GNU Lesser General Public License
# version 3 along with OpenOffice.org.  If not, see
# <http://www.openoffice.org/license.html>
# for a copy of the LGPLv3 License.
#
#*************************************************************************

$(eval $(call gb_Library_Library,sdbt))
$(eval $(call gb_Library_add_package_headers,sdbt,dbaccess_inc))


$(eval $(call gb_Library_set_componentfile,sdbt,dbaccess/util/sdbt))

$(eval $(call gb_Library_add_api,sdbt,\
	udkapi \
	offapi \
))

$(eval $(call gb_Library_set_include,sdbt,\
	-I$(SRCDIR)/dbaccess/inc \
	-I$(SRCDIR)/dbaccess/source/sdbtools/inc \
	-I$(SRCDIR)/dbaccess/source/inc \
	-I$(SRCDIR)/dbaccess/inc/pch \
	$$(INCLUDE) \
))

$(eval $(call gb_Library_add_linked_libs,sdbt,\
	tl \
	cppuhelper \
	cppu \
	comphelper \
	utl \
	sal \
	stl \
	dbtools \
    $(gb_STDLIBS) \
))

$(eval $(call gb_Library_add_exception_objects,sdbt,\
	dbaccess/source/sdbtools/connection/connectiontools \
	dbaccess/source/sdbtools/connection/tablename \
	dbaccess/source/sdbtools/connection/objectnames \
	dbaccess/source/sdbtools/connection/datasourcemetadata \
	dbaccess/source/sdbtools/misc/sdbt_services \
	dbaccess/source/sdbtools/misc/module_sdbt \
))

$(eval $(call gb_Library_add_noexception_objects,sdbt, \
	dbaccess/source/shared/sdbtstrings \
))

# vim: set noet sw=4 ts=4: