/***************************************************************************
                          QgsProcessingModelFileTransform.cpp
                             -------------------
    begin                : October 2026
    copyright            : (C) 2026 by Celia Buira
    email                : celia dot buira at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsprocessingmodelfiletransform.h"

#include "qgslogger.h"
#include "qgsproject.h"
#include "qgsprojectproperty.h"
#include "qgsprojectversion.h"
#include "qgsrasterdataprovider.h"
#include "qgssymbollayerutils.h"
#include "qgsxmlutils.h"

#include <QDomDocument>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>

using namespace Qt::StringLiterals;


// Transformer functions below. Declare functions here,
// define them in QgsProcessingModelFileTransform.cpp and add them
// to the transformArray with proper version number
void transformNull( QgsProcessingModelFileTransform *pft )
{
  Q_UNUSED( pft )
} // Do absolutely nothing
void transform0000to4400( QgsProcessingModelFileTransform *pft );

typedef struct
{
    QgsProjectVersion from;
    QgsProjectVersion to;
    void ( *transformFunc )( QgsProcessingModelFileTransform * );
} TransformItem;

typedef std::vector<TransformItem> Transformers;

bool QgsProcessingModelFileTransform::updateRevision( const QgsProjectVersion &newVersion )
{
  Q_UNUSED( newVersion )
  bool returnValue = false;

  static const Transformers transformers( {
    { QgsProjectVersion( 0, 0, 0 ), QgsProjectVersion( 4, 2, 0 ), &transform0000to4400 },
  } );

  if ( !mDom.isNull() )
  {
    for ( const TransformItem &transformer : transformers )
    {
      if ( transformer.to >= mCurrentVersion && ( transformer.from == mCurrentVersion || transformer.from.isNull() ) )
      {
        // Run the transformer, and update the revision in every case
        ( *( transformer.transformFunc ) )( this );
        mCurrentVersion = transformer.to;
        returnValue = true;
      }
    }
  }

#if 0
  dump();
#endif
  return returnValue;
}

void QgsProcessingModelFileTransform::dump()
{
  QgsDebugMsgLevel( u"Current project file version is %1.%2.%3"_s.arg( mCurrentVersion.majorVersion() ).arg( mCurrentVersion.minorVersion() ).arg( mCurrentVersion.subVersion() ), 1 );
#ifdef QGISDEBUG
  // Using QgsDebugMsgLevel() didn't print the entire pft->dom()...
  std::cout << mDom.toString( 2 ).toLatin1().constData(); // OK
#endif
}

/*
 *  Transformers below!
 */


void transform0000to4400( QgsProcessingModelFileTransform *pft )
{
  //Place holder
  const QDomNodeList OptionList = pft->dom().elementsByTagName( u"Option"_s );
  for ( int i = 0; i < OptionList.size(); ++i )
  {
    QDomElement option = OptionList.at( i ).toElement();
    option.setAttribute( u"anchorPoint"_s, QString::number( 4 ) );
  }
}


QDomDocument &QgsProcessingModelFileTransform::dom()
{
  return mDom;
}

QgsProjectVersion QgsProcessingModelFileTransform::currentVersion() const
{
  return mCurrentVersion;
}
