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

#include "qmakemanager.h"

#include <QAction>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QList>


#include <KUrl>
#include <KIO//Job>
#include <KDirWatch>
#include <QIcon>
#include <KPluginFactory>
#include <KPluginLoader>

#include <interfaces/icore.h>
#include <interfaces/iproject.h>
#include <interfaces/contextmenuextension.h>
#include <interfaces/context.h>
#include <interfaces/iruncontroller.h>
#include <interfaces/iproject.h>
#include <interfaces/iplugincontroller.h>
#include <project/projectmodel.h>
#include <serialization/indexedstring.h>

#include "qmakebuilder/iqmakebuilder.h"

#include "qmakemodelitems.h"
#include "qmakeprojectfile.h"
#include "qmakecache.h"
#include "qmakemkspecs.h"
#include "qmakejob.h"
#include "qmakebuilddirchooserdialog.h"
#include "qmakeconfig.h"
#include <KDirWatch>
#include <interfaces/iprojectcontroller.h>
#include "debug.h"

using namespace KDevelop;

//BEGIN Helpers

QMakeFolderItem* findQMakeFolderParent(ProjectBaseItem* item) {
    QMakeFolderItem* p = nullptr;
    while (!p && item) {
        p = dynamic_cast<QMakeFolderItem*>( item );
        item = item->parent();
    }
    return p;
}

//END Helpers

K_PLUGIN_FACTORY_WITH_JSON(QMakeSupportFactory, "kdevqmakemanager.json", registerPlugin<QMakeProjectManager>(); )

QMakeProjectManager* QMakeProjectManager::m_self = nullptr;

QMakeProjectManager* QMakeProjectManager::self()
{
    return m_self;
}

QMakeProjectManager::QMakeProjectManager( QObject* parent, const QVariantList& )
        : AbstractFileManagerPlugin("kdevqmakemanager", parent),
          IBuildSystemManager(),
          m_builder(nullptr),
          m_runQmake(nullptr)
{
    Q_ASSERT(!m_self);
    m_self = this;

    KDEV_USE_EXTENSION_INTERFACE( IBuildSystemManager )
    IPlugin* i = core()->pluginController()->pluginForExtension( "org.kdevelop.IQMakeBuilder" );
    Q_ASSERT(i);
    m_builder = i->extension<IQMakeBuilder>();
    Q_ASSERT(m_builder);

    connect(this, SIGNAL(folderAdded(KDevelop::ProjectFolderItem*)),
            this, SLOT(slotFolderAdded(KDevelop::ProjectFolderItem*)));

    m_runQmake = new QAction(QIcon::fromTheme("qtlogo"), i18n("Run QMake"), this);
    connect(m_runQmake, SIGNAL(triggered(bool)),
            this, SLOT(slotRunQMake()));
}

QMakeProjectManager::~QMakeProjectManager()
{
    m_self = nullptr;
}

IProjectFileManager::Features QMakeProjectManager::features() const
{
    return Features(Folders | Targets | Files);
}

bool QMakeProjectManager::isValid( const Path& path, const bool isFolder, IProject* project ) const
{
    if (!isFolder && path.lastPathSegment().startsWith("Makefile") ) {
        return false;
    }
    return AbstractFileManagerPlugin::isValid(path, isFolder, project);
}

Path QMakeProjectManager::buildDirectory(ProjectBaseItem* item) const
{
    ///TODO: support includes by some other parent or sibling in a different file-tree-branch
    QMakeFolderItem* qmakeItem = findQMakeFolderParent(item);
    Path dir;
    if ( qmakeItem ) {
        if (!qmakeItem->parent()) {
            // build root item
            dir = QMakeConfig::buildDirFromSrc(qmakeItem->project(), qmakeItem->path());
        } else {
            // build sub-item
            foreach ( QMakeProjectFile* pro, qmakeItem->projectFiles() ) {
                if ( QDir(pro->absoluteDir()) == QFileInfo(qmakeItem->path().toUrl().toLocalFile()).absoluteDir() ||
                    pro->hasSubProject( qmakeItem->path().toUrl().toLocalFile() ) ) {
                    // get path from project root and it to buildDir
                    dir = QMakeConfig::buildDirFromSrc( qmakeItem->project(), Path(pro->absoluteDir()) );
                    break;
                }
            }
        }
    }

    qCDebug(KDEV_QMAKE) << "build dir for" << item->text() << item->path() << "is:" << dir;
    return dir;
}

ProjectFolderItem* QMakeProjectManager::createFolderItem( IProject* project, const Path& path,
                                                          ProjectBaseItem* parent )
{
    if ( !parent ) {
        return projectRootItem( project, path );
    } else if (ProjectFolderItem* buildFolder = buildFolderItem( project, path, parent )) {
        // child folder in a qmake folder
        return buildFolder;
    } else {
        return AbstractFileManagerPlugin::createFolderItem( project, path, parent );
    }
}

ProjectFolderItem* QMakeProjectManager::projectRootItem( IProject* project, const Path& path )
{
    QFileInfo fi( path.toLocalFile() );
    QDir dir( path.toLocalFile() );
    QStringList l = dir.entryList( QStringList() << "*.pro" );

    QString projectfile;

    if( l.count() && l.indexOf( project->name() + ".pro") != -1 )
        projectfile = project->name() + ".pro";
    if( l.isEmpty() || ( l.count() && l.indexOf( fi.baseName() + ".pro" ) != -1 ) )
    {
        projectfile = fi.baseName() + ".pro";
    }else
    {
        projectfile = l.first();
    }

    QHash<QString,QString> qmvars = queryQMake( project );
    const QString mkSpecFile = QMakeConfig::findBasicMkSpec( qmvars );
    Q_ASSERT(!mkSpecFile.isEmpty());
    QMakeMkSpecs* mkspecs = new QMakeMkSpecs( mkSpecFile, qmvars );
    mkspecs->setProject( project );
    mkspecs->read();
    QMakeCache* cache = findQMakeCache( project );
    if( cache ) {
        cache->setMkSpecs( mkspecs );
        cache->read();
    }
    Path proPath(path, projectfile);
    /// TODO: use Path in QMakeProjectFile
    QMakeProjectFile* scope = new QMakeProjectFile( proPath.toLocalFile() );
    scope->setProject( project );
    scope->setMkSpecs( mkspecs );
    if( cache ) {
        scope->setQMakeCache( cache );
    }
    scope->read();
    qCDebug(KDEV_QMAKE) << "top-level scope with variables:" << scope->variables();
    auto  item = new QMakeFolderItem( project, path );
    item->addProjectFile(scope);
    return item;
}

ProjectFolderItem* QMakeProjectManager::buildFolderItem( IProject* project, const Path& path,
                                                         ProjectBaseItem* parent )
{
    // find .pro or .pri files in dir
    QDir dir(path.toLocalFile());
    QStringList projectFiles = dir.entryList(QStringList() << "*.pro" << "*.pri", QDir::Files);
    if ( projectFiles.isEmpty() ) {
        return nullptr;
    }

    auto  folderItem = new QMakeFolderItem(project, path, parent);

    //TODO: included by not-parent file (in a nother file-tree-branch).
    QMakeFolderItem* qmakeParent = findQMakeFolderParent(parent);
    if (!qmakeParent) {
        // happens for bad qmake configurations
        return nullptr;
    }

    foreach( const QString& file, projectFiles ) {
        const QString absFile = dir.absoluteFilePath(file);

        //TODO: multiple includes by different .pro's
        QMakeProjectFile* parentPro = nullptr;
        foreach( QMakeProjectFile* p, qmakeParent->projectFiles() ) {
            if (p->hasSubProject(absFile)) {
                parentPro = p;
                break;
            }
        }
        if (!parentPro && file.endsWith(".pri")) {
            continue;
        }
        qCDebug(KDEV_QMAKE) << "add project file:" << absFile;
        if (parentPro) {
            qCDebug(KDEV_QMAKE) << "parent:" << parentPro->absoluteFile();
        } else {
            qCDebug(KDEV_QMAKE) << "no parent, assume project root";
        }

        auto  qmscope = new QMakeProjectFile( absFile );
        qmscope->setProject( project );

        const QFileInfo info( absFile );
        const QDir d = info.dir();
        ///TODO: cleanup
        if ( parentPro) {
            // subdir
            if( QMakeCache* cache = findQMakeCache(project, Path(d.canonicalPath())) ) {
                cache->setMkSpecs( parentPro->mkSpecs() );
                cache->read();
                qmscope->setQMakeCache( cache );
            } else {
                qmscope->setQMakeCache( parentPro->qmakeCache() );
            }

            qmscope->setMkSpecs( parentPro->mkSpecs() );
        } else {
            // new project
            QMakeFolderItem* root = dynamic_cast<QMakeFolderItem*>( project->projectItem() );
            Q_ASSERT(root);
            qmscope->setMkSpecs( root->projectFiles().first()->mkSpecs() );
            if( root->projectFiles().first()->qmakeCache() ) {
                qmscope->setQMakeCache( root->projectFiles().first()->qmakeCache() );
            }
        }

        if( qmscope->read() ) {
            //TODO: only on read?
            folderItem->addProjectFile( qmscope );
        } else {
            delete qmscope;
            return nullptr;
        }
    }

    return folderItem;
}

void QMakeProjectManager::slotFolderAdded( ProjectFolderItem* folder )
{
    QMakeFolderItem* qmakeParent = dynamic_cast<QMakeFolderItem*>( folder );
    if ( !qmakeParent ) {
        return;
    }

    qCDebug(KDEV_QMAKE) << "adding targets for" << folder->path();
    foreach( QMakeProjectFile* pro, qmakeParent->projectFiles() ) {
        foreach( const QString& s, pro->targets() ) {
            if (!isValid(Path(folder->path(), s), false, folder->project())) {
                continue;
            }
            qCDebug(KDEV_QMAKE) << "adding target:" << s;
            Q_ASSERT(!s.isEmpty());
            auto  target = new QMakeTargetItem( pro, folder->project(), s, folder );
            foreach( const QString& path, pro->filesForTarget(s) ) {
                new ProjectFileItem( folder->project(), Path(path), target );
                ///TODO: signal?
            }
        }
    }
}

ProjectFolderItem* QMakeProjectManager::import( IProject* project )
{
    const Path dirName = project->path();
    if( dirName.isRemote() )
    {
        //FIXME turn this into a real warning
        qCWarning(KDEV_QMAKE) << "not a local file. QMake support doesn't handle remote projects";
        return nullptr;
    }

    while (projectNeedsConfiguration(project)) {
        QMakeBuildDirChooserDialog chooser(project);
        if(chooser.exec() == QDialog::Rejected) {
            qDebug() << "User stopped project import";
            //TODO: return 0 has no effect.
            return nullptr;
        }
    }

    ProjectFolderItem* ret = AbstractFileManagerPlugin::import( project );

    connect(projectWatcher(project), SIGNAL(dirty(QString)),
            this, SLOT(slotDirty(QString)));

    return ret;
}

void QMakeProjectManager::slotDirty(const QString& path)
{
    if (!path.endsWith(".pro") && !path.endsWith(".pri")) {
        return;
    }

    QFileInfo info(path);
    if (!info.isFile()) {
        return;
    }

    ///FIXME: use Path
    const KUrl url(path);
    if (!isValid(Path(url), false, nullptr)) {
        return;
    }

    IProject* project = ICore::self()->projectController()->findProjectForUrl(url);
    if (!project) {
        // this can happen when we create/remove lots of files in a
        // sub dir of a project - ignore such cases for now
        return;
    }

    bool finished = false;
    foreach(ProjectFolderItem* folder, project->foldersForPath(IndexedString(url.upUrl()))) {
        if (QMakeFolderItem* qmakeFolder = dynamic_cast<QMakeFolderItem*>( folder )) {
            foreach(QMakeProjectFile* pro, qmakeFolder->projectFiles()) {
                if (pro->absoluteFile() == path) {
                    //TODO: children
                    //TODO: cache added
                    qDebug() << "reloading" << pro << path;
                    pro->read();
                }
            }
            finished = true;
        } else if (ProjectFolderItem* newFolder = buildFolderItem(project, folder->path(), folder->parent())) {
            qDebug() << "changing from normal folder to qmake project folder:" << folder->path().toUrl();
            // .pro / .pri file did not exist before
            while(folder->rowCount()) {
                newFolder->appendRow(folder->takeRow(0));
            }
            folder->parent()->removeRow(folder->row());
            folder = newFolder;
            finished = true;
        }
        if (finished) {
            // remove existing targets and readd them
            for(int i = 0; i < folder->rowCount(); ++i) {
                if (folder->child(i)->target()) {
                    folder->removeRow(i);
                }
            }
            ///TODO: put into it's own function once we add more stuff to that slot
            slotFolderAdded(folder);
            break;
        }
    }
}

QList<ProjectTargetItem*> QMakeProjectManager::targets(ProjectFolderItem* item) const
{
    Q_UNUSED(item)
    return QList<ProjectTargetItem*>();
}

IProjectBuilder* QMakeProjectManager::builder() const
{
    Q_ASSERT(m_builder);
    return m_builder;
}

Path::List QMakeProjectManager::includeDirectories(ProjectBaseItem* item) const
{
    Path::List list;
    QMakeFolderItem* folder = findQMakeFolderParent(item);
    if ( folder ) {
        foreach( QMakeProjectFile* pro, folder->projectFiles() ) {
            if (pro->files().contains(item->path().toLocalFile())) {
                foreach(const QString& dir, pro->includeDirectories()) {
                    Path path(dir);
                    if (!list.contains(path)) {
                        list << path;
                    }
                }
            }
        }
        if (list.isEmpty()) {
            // fallback for new files, use all possible include dirs
            foreach( QMakeProjectFile* pro, folder->projectFiles() ) {
                foreach(const QString& dir, pro->includeDirectories()) {
                    Path path(dir);
                    if (!list.contains(path)) {
                        list << path;
                    }
                }
            }
        }
        // make sure the base dir is included
        if (!list.contains(folder->path())) {
            list << folder->path();
        }
//         qCDebug(KDEV_QMAKE) << "include dirs for" << item->path() << ":" << list;
    }
    return list;
}

QHash< QString, QString > QMakeProjectManager::defines(ProjectBaseItem* item) const
{
    QHash<QString,QString> d;
    QMakeFolderItem *folder = findQMakeFolderParent(item);
    if (!folder) {
        // happens for bad qmake configurations
        return d;
    }
    foreach(QMakeProjectFile *pro, folder->projectFiles()) {
        foreach(QMakeProjectFile::DefinePair def, pro->defines()) {
            d.insert(def.first, def.second);
        }
    }
    return d;
}

bool QMakeProjectManager::hasIncludesOrDefines(KDevelop::ProjectBaseItem* item) const
{
    return findQMakeFolderParent(item);
}

QHash<QString,QString> QMakeProjectManager::queryQMake( IProject* project ) const
{
    if( !project->path().toUrl().isLocalFile() || !m_builder )
        return QHash<QString,QString>();

    return QMakeConfig::queryQMake(QMakeConfig::qmakeBinary( project ));
}

QMakeCache* QMakeProjectManager::findQMakeCache( IProject* project, const Path& path ) const
{
    QDir curdir( QMakeConfig::buildDirFromSrc(project, !path.isValid() ? project->path() : path).toLocalFile() );
    curdir.makeAbsolute();
    while( !curdir.exists(".qmake.cache") && !curdir.isRoot() && curdir.cdUp() )
    {
        qDebug() << curdir;
    }

    if( curdir.exists(".qmake.cache") )
    {
        qDebug() << "Found QMake cache in " << curdir.absolutePath();
        return new QMakeCache( curdir.canonicalPath()+"/.qmake.cache" );
    }
    return nullptr;
}

ContextMenuExtension QMakeProjectManager::contextMenuExtension( Context* context )
{
    ContextMenuExtension ext;

    if ( context->hasType( Context::ProjectItemContext ) ) {
        ProjectItemContext* pic = dynamic_cast<ProjectItemContext*>( context );
        Q_ASSERT(pic);
        if ( pic->items().isEmpty() ) {
            return ext;
        }

        m_actionItem = dynamic_cast<QMakeFolderItem*>( pic->items().first() );
        if ( m_actionItem ) {
            ext.addAction( ContextMenuExtension::ProjectGroup, m_runQmake );
        }
    }

    return ext;
}

void QMakeProjectManager::slotRunQMake()
{
    Q_ASSERT(m_actionItem);

    Path srcDir = m_actionItem->path();
    Path buildDir = QMakeConfig::buildDirFromSrc(m_actionItem->project(), srcDir);
    QMakeJob* job = new QMakeJob( srcDir.toLocalFile(), buildDir.toLocalFile(), this );

    job->setQMakePath(QMakeConfig::qmakeBinary(m_actionItem->project()));

    KConfigGroup cg(m_actionItem->project()->projectConfiguration(), QMakeConfig::CONFIG_GROUP);
    QString installPrefix = cg.readEntry(QMakeConfig::INSTALL_PREFIX, QString());
    if(!installPrefix.isEmpty())
        job->setInstallPrefix(installPrefix);
    job->setBuildType( cg.readEntry<int>(QMakeConfig::BUILD_TYPE, 0) );
    job->setExtraArguments( cg.readEntry(QMakeConfig::EXTRA_ARGUMENTS, QString()) );

    KDevelop::ICore::self()->runController()->registerJob( job );
}

bool QMakeProjectManager::projectNeedsConfiguration(IProject* project)
{
    if (!QMakeConfig::isConfigured(project)) {
        return true;
    }
    const QString qmakeBinary = QMakeConfig::qmakeBinary( project );
    if (qmakeBinary.isEmpty()) {
        return true;
    }
    const QHash<QString, QString> vars = queryQMake(project);
    if (vars.isEmpty()) {
        return true;
    }
    if (QMakeConfig::findBasicMkSpec(vars).isEmpty()) {
        return true;
    }
    if (!QMakeConfig::buildDirFromSrc(project, project->path()).isValid()) {
        return true;
    }
    return false;
}

#include "qmakemanager.moc"
