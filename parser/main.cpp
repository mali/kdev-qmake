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

#include "qmakedriver.h"

#include <QtCore/QString>


#include <kurl.h>
#include <QCommandLineParser>
#include <QCommandLineOption>

int main( int argc, char* argv[] )
{
    KAboutData aboutData( QLatin1String("QMake Parser"), i18n("qmake-parser"), QLatin1String("4.0.0"));
    aboutData.setShortDescription(i18n("Parse QMake project files"));
    QApplication app(argc, argv);
    QCommandLineParser parser;
    KAboutData::setApplicationData(aboutData);
    parser.addVersionOption();
    parser.addHelpOption();
    //PORTING SCRIPT: adapt aboutdata variable if necessary
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("!debug"), i18n("Enable output of the debug AST")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("!+files"), i18n("QMake project files")));


    if( parser.positionalArguments().count() < 1 )
    {
        KCmdLineArgs::usage(0);
    }

    int debug = 0;
    if( parser.isSet("debug") )
        debug = 1;
    for( int i = 0 ; i < parser.positionalArguments().count() ; i++ )
    {
        QMake::Driver d;
        if( !d.readFile( args->url(i).toLocalFile() ) )
            exit( EXIT_FAILURE );
        d.setDebug( debug );

        QMake::ProjectAST* ast = 0;
        if ( !d.parse( &ast ) ) {
            exit( EXIT_FAILURE );
        }else
        {
        }
    }
    return EXIT_SUCCESS;
}

