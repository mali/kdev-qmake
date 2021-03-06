/*****************************************************************************
 * Copyright (c) 2007 Piyush verma <piyush.verma@gmail.com>                  *
 * Copyright (c) 2010 Milian Wolff <mail@milianw.de>                         *
 *                                                                           *
 * Permission is hereby granted, free of charge, to any person obtaining     *
 * a copy of this software and associated documentation files (the           *
 * "Software"), to deal in the Software without restriction, including       *
 * without limitation the rights to use, copy, modify, merge, publish,       *
 * distribute, sublicense, and/or sell copies of the Software, and to        *
 * permit persons to whom the Software is furnished to do so, subject to     *
 * the following conditions:                                                 *
 *                                                                           *
 * The above copyright notice and this permission notice shall be            *
 * included in all copies or substantial portions of the Software.           *
 *                                                                           *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,           *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF        *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                     *
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE    *
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION    *
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION     *
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.           *
 *****************************************************************************/
#include "qmakeeditorintegrator.h"

#include "parser/ast.h"

using namespace KDevelop;

QMakeEditorIntegrator::QMakeEditorIntegrator()
{
}

CursorInRevision QMakeEditorIntegrator::findPosition( QMake::AST* node , Edge edge ) const
{
    if ( edge == BackEdge )
    {
        // Apparently KTE expects a range to go until _after_ the last character that should be included
        // however the parser calculates endCol as the index _before_ the last included character, so adjust here
        // Maybe this should be solved differently, ask David how C++ support does it or look it up
        return CursorInRevision( node->endLine, node->endColumn+1 );
    }else
    {
        return CursorInRevision( node->startLine, node->startColumn );
    }
}

RangeInRevision QMakeEditorIntegrator::findRange( QMake::AST * node )
{
    return RangeInRevision( findPosition( node, FrontEdge ), findPosition( node, BackEdge ) );
}

RangeInRevision QMakeEditorIntegrator::findRange( QMake::AST* from, QMake::AST* to )
{
    return RangeInRevision( findPosition( from, FrontEdge ), findPosition( to, BackEdge ) );
}

