/* This file is part of KDevelop
    Copyright (C) 2007 Andreas Pakulat <apaku@gmx.de>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#ifndef IQMAKEBUILDER_H
#define IQMAKEBUILDER_H

#include "iprojectbuilder.h"
#include "iextension.h"
#include <QtDesigner/QAbstractExtensionFactory>

class IProject;
class ProjectItem;

/**
@author Andreas Pakulat
*/

class IQMakeBuilder : public KDevelop::IProjectBuilder
{
public:

    virtual ~IQMakeBuilder() {}

};

Q_DECLARE_EXTENSION_INTERFACE( IQMakeBuilder, "org.kdevelop.IQMakeBuilder" )

#endif
//kate: space-indent on; indent-width 4; replace-tabs on; auto-insert-doxygen on; indent-mode cstyle;