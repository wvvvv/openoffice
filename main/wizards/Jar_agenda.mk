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

$(eval $(call gb_Jar_Jar,agenda,SRCDIR))

$(eval $(call gb_Jar_add_jars,agenda,\
	$(OUTDIR)/bin/ridl.jar \
	$(OUTDIR)/bin/unoil.jar \
	$(OUTDIR)/bin/jurt.jar \
	$(OUTDIR)/bin/juh.jar \
	$(OUTDIR)/bin/java_uno.jar \
	$(OUTDIR)/bin/commonwizards.jar \
))

$(eval $(call gb_Jar_set_manifest,agenda,$(SRCDIR)/wizards/com/sun/star/wizards/agenda/MANIFEST.MF))

$(eval $(call gb_Jar_set_jarclasspath,agenda,\
	commonwizards.jar \
))

$(eval $(call gb_Jar_set_packageroot,agenda,com))

$(eval $(call gb_Jar_add_sourcefiles,agenda,\
	wizards/com/sun/star/wizards/agenda/AgendaTemplate \
	wizards/com/sun/star/wizards/agenda/AgendaWizardDialog \
	wizards/com/sun/star/wizards/agenda/AgendaWizardDialogConst \
	wizards/com/sun/star/wizards/agenda/AgendaWizardDialogImpl \
	wizards/com/sun/star/wizards/agenda/AgendaWizardDialogResources \
	wizards/com/sun/star/wizards/agenda/CallWizard \
	wizards/com/sun/star/wizards/agenda/CGAgenda \
	wizards/com/sun/star/wizards/agenda/CGTopic \
	wizards/com/sun/star/wizards/agenda/TemplateConsts \
	wizards/com/sun/star/wizards/agenda/TopicsControl \
))

$(eval $(call gb_Jar_set_componentfile,agenda,wizards/com/sun/star/wizards/agenda/agenda,OOO))

# vim: set noet sw=4 ts=4: