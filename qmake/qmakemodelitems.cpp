/* KDevelop QMake Support
 *
 * Copyright 2006 Andreas Pakulat <apaku@gmx.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "qmakemodelitems.h"

#include "qmakeprojectscope.h"

QMakeFolderItem::QMakeFolderItem( QMakeProjectScope* scope, const KUrl& url, QStandardItem* parent )
: KDevProjectFolderItem( url, parent ), m_projectScope( scope )
{
}

QMakeProjectScope* QMakeFolderItem::projectScope() const
{
    return m_projectScope;
}

QMakeFolderItem::~QMakeFolderItem()
{
}

QMakeTargetItem::QMakeTargetItem( const QString& s, QStandardItem* parent )
    : KDevProjectTargetItem( s, parent )
{
}

QMakeTargetItem::~QMakeTargetItem()
{
}

const KUrl::List& QMakeTargetItem::includeDirectories() const
{
    return m_includes;
}

const QHash<QString, QString>& QMakeTargetItem::environment() const
{
    return m_env;
}

const DomUtil::PairList& QMakeTargetItem::defines() const
{
    return m_defs;
}

// kate: indent-mode cstyle; space-indent on; indent-width 4; replace-tabs on;