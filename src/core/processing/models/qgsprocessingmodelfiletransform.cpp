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


#include <cstdlib>

#include "QgsProcessingModelFileTransform.h"
#include "qgslogger.h"
#include "qgsmasksymbollayer.h"
#include "qgspathresolver.h"
#include "qgsproject.h"
#include "qgsprojectproperty.h"
#include "qgsprojectversion.h"
#include "qgsrasterbandstats.h"
#include "qgsrasterdataprovider.h"
#include "qgsrasterlayer.h"
#include "qgsreadwritecontext.h"
#include "qgsstyleentityvisitor.h"
#include "qgssymbollayerreference.h"
#include "qgssymbollayerutils.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayer.h"
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
void transform2200to2300( QgsProcessingModelFileTransform *pft );
void transform3000( QgsProcessingModelFileTransform *pft );

//helper functions
void transformContrastEnhancement( QDomDocument &doc, const QDomElement &rasterproperties, QDomElement &rendererElem );
void transformRasterTransparency( QDomDocument &doc, const QDomElement &orig, QDomElement &rendererElem );

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
    { QgsProjectVersion( 2, 1, 0 ), QgsProjectVersion( 2, 2, 0 ), &transformNull },
    { QgsProjectVersion( 2, 2, 0 ), QgsProjectVersion( 2, 3, 0 ), &transform2200to2300 },
    // A transformer with a NULL from version means that it should be run when upgrading
    // from any version and will take care that it's not going to cause trouble if it's
    // run several times on the same file.
    { QgsProjectVersion(), QgsProjectVersion( 3, 0, 0 ), &transform3000 },
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

void transform2200to2300( QgsProcessingModelFileTransform *pft )
{
  //composer: set placement for all picture items to middle, to mimic <=2.2 behavior
  const QDomNodeList composerPictureList = pft->dom().elementsByTagName( u"ComposerPicture"_s );
  for ( int i = 0; i < composerPictureList.size(); ++i )
  {
    QDomElement picture = composerPictureList.at( i ).toElement();
    picture.setAttribute( u"anchorPoint"_s, QString::number( 4 ) );
  }
}

void transform3000( QgsProcessingModelFileTransform *pft )
{
  // transform OTF off to "no projection" for project
  QDomElement propsElem = pft->dom().firstChildElement( u"qgis"_s ).toElement().firstChildElement( u"properties"_s );
  if ( !propsElem.isNull() )
  {
    const QDomNodeList srsNodes = propsElem.elementsByTagName( u"SpatialRefSys"_s );
    QDomElement srsElem;
    QDomElement projElem;
    if ( srsNodes.count() > 0 )
    {
      srsElem = srsNodes.at( 0 ).toElement();
      const QDomNodeList projNodes = srsElem.elementsByTagName( u"ProjectionsEnabled"_s );
      if ( projNodes.count() == 0 )
      {
        projElem = pft->dom().createElement( u"ProjectionsEnabled"_s );
        projElem.setAttribute( u"type"_s, u"int"_s );
        const QDomText projText = pft->dom().createTextNode( u"0"_s );
        projElem.appendChild( projText );
        srsElem.appendChild( projElem );
      }
    }
    else
    {
      srsElem = pft->dom().createElement( u"SpatialRefSys"_s );
      projElem = pft->dom().createElement( u"ProjectionsEnabled"_s );
      projElem.setAttribute( u"type"_s, u"int"_s );
      const QDomText projText = pft->dom().createTextNode( u"0"_s );
      projElem.appendChild( projText );
      srsElem.appendChild( projElem );
      propsElem.appendChild( srsElem );
    }

    // transform map canvas CRS to project CRS - this is because project CRS was inconsistently used
    // prior to 3.0. In >= 3.0 main canvas CRS is forced to match project CRS, so we need to make
    // sure we can read the project CRS correctly
    const QDomNodeList canvasNodes = pft->dom().elementsByTagName( u"mapcanvas"_s );
    if ( canvasNodes.count() > 0 )
    {
      const QDomElement canvasElem = canvasNodes.at( 0 ).toElement();
      const QDomNodeList canvasSrsNodes = canvasElem.elementsByTagName( u"spatialrefsys"_s );
      if ( canvasSrsNodes.count() > 0 )
      {
        const QDomElement canvasSrsElem = canvasSrsNodes.at( 0 ).toElement();
        QString proj;
        QString authid;
        QString srsid;

        const QDomNodeList proj4Nodes = canvasSrsElem.elementsByTagName( u"proj4"_s );
        if ( proj4Nodes.count() > 0 )
        {
          const QDomElement proj4Node = proj4Nodes.at( 0 ).toElement();
          proj = proj4Node.text();
        }
        const QDomNodeList authidNodes = canvasSrsElem.elementsByTagName( u"authid"_s );
        if ( authidNodes.count() > 0 )
        {
          const QDomElement authidNode = authidNodes.at( 0 ).toElement();
          authid = authidNode.text();
        }
        const QDomNodeList srsidNodes = canvasSrsElem.elementsByTagName( u"srsid"_s );
        if ( srsidNodes.count() > 0 )
        {
          const QDomElement srsidNode = srsidNodes.at( 0 ).toElement();
          srsid = srsidNode.text();
        }

        // clear existing project CRS nodes
        const QDomNodeList oldProjectProj4Nodes = srsElem.elementsByTagName( u"ProjectCRSProj4String"_s );
        for ( int i = oldProjectProj4Nodes.count(); i >= 0; --i )
        {
          srsElem.removeChild( oldProjectProj4Nodes.at( i ) );
        }
        const QDomNodeList oldProjectCrsNodes = srsElem.elementsByTagName( u"ProjectCrs"_s );
        for ( int i = oldProjectCrsNodes.count(); i >= 0; --i )
        {
          srsElem.removeChild( oldProjectCrsNodes.at( i ) );
        }
        const QDomNodeList oldProjectCrsIdNodes = srsElem.elementsByTagName( u"ProjectCRSID"_s );
        for ( int i = oldProjectCrsIdNodes.count(); i >= 0; --i )
        {
          srsElem.removeChild( oldProjectCrsIdNodes.at( i ) );
        }
        const QDomNodeList projectionsEnabledNodes = srsElem.elementsByTagName( u"ProjectionsEnabled"_s );
        for ( int i = projectionsEnabledNodes.count(); i >= 0; --i )
        {
          srsElem.removeChild( projectionsEnabledNodes.at( i ) );
        }

        QDomElement proj4Elem = pft->dom().createElement( u"ProjectCRSProj4String"_s );
        proj4Elem.setAttribute( u"type"_s, u"QString"_s );
        const QDomText proj4Text = pft->dom().createTextNode( proj );
        proj4Elem.appendChild( proj4Text );
        QDomElement projectCrsElem = pft->dom().createElement( u"ProjectCrs"_s );
        projectCrsElem.setAttribute( u"type"_s, u"QString"_s );
        const QDomText projectCrsText = pft->dom().createTextNode( authid );
        projectCrsElem.appendChild( projectCrsText );
        QDomElement projectCrsIdElem = pft->dom().createElement( u"ProjectCRSID"_s );
        projectCrsIdElem.setAttribute( u"type"_s, u"int"_s );
        const QDomText srsidText = pft->dom().createTextNode( srsid );
        projectCrsIdElem.appendChild( srsidText );
        QDomElement projectionsEnabledElem = pft->dom().createElement( u"ProjectionsEnabled"_s );
        projectionsEnabledElem.setAttribute( u"type"_s, u"int"_s );
        const QDomText projectionsEnabledText = pft->dom().createTextNode( u"1"_s );
        projectionsEnabledElem.appendChild( projectionsEnabledText );
        srsElem.appendChild( proj4Elem );
        srsElem.appendChild( projectCrsElem );
        srsElem.appendChild( projectCrsIdElem );
        srsElem.appendChild( projectionsEnabledElem );

        const QDomNodeList srsNodes = propsElem.elementsByTagName( u"SpatialRefSys"_s );
        for ( int i = srsNodes.count(); i >= 0; --i )
        {
          propsElem.removeChild( srsNodes.at( i ) );
        }
        propsElem.appendChild( srsElem );
      }
    }
  }


  const QDomNodeList mapLayers = pft->dom().elementsByTagName( u"maplayer"_s );

  for ( int mapLayerIndex = 0; mapLayerIndex < mapLayers.count(); ++mapLayerIndex )
  {
    QDomElement layerElem = mapLayers.at( mapLayerIndex ).toElement();

    // The newly added fieldConfiguration element
    QDomElement fieldConfigurationElement = pft->dom().createElement( u"fieldConfiguration"_s );
    layerElem.appendChild( fieldConfigurationElement );

    const QDomNodeList editTypeNodes = layerElem.namedItem( u"edittypes"_s ).childNodes();
    QDomElement constraintExpressionsElem = pft->dom().createElement( u"constraintExpressions"_s );
    layerElem.appendChild( constraintExpressionsElem );

    for ( int i = 0; i < editTypeNodes.size(); ++i )
    {
      const QDomNode editTypeNode = editTypeNodes.at( i );
      const QDomElement editTypeElement = editTypeNode.toElement();

      QDomElement fieldElement = pft->dom().createElement( u"field"_s );
      fieldConfigurationElement.appendChild( fieldElement );

      const QString name = editTypeElement.attribute( u"name"_s );
      fieldElement.setAttribute( u"name"_s, name );
      QDomElement constraintExpressionElem = pft->dom().createElement( u"constraint"_s );
      constraintExpressionElem.setAttribute( u"field"_s, name );
      constraintExpressionsElem.appendChild( constraintExpressionElem );

      QDomElement editWidgetElement = pft->dom().createElement( u"editWidget"_s );
      fieldElement.appendChild( editWidgetElement );

      const QString ewv2Type = editTypeElement.attribute( u"widgetv2type"_s );
      editWidgetElement.setAttribute( u"type"_s, ewv2Type );

      const QDomElement ewv2CfgElem = editTypeElement.namedItem( u"widgetv2config"_s ).toElement();

      if ( !ewv2CfgElem.isNull() )
      {
        QDomElement editWidgetConfigElement = pft->dom().createElement( u"config"_s );
        editWidgetElement.appendChild( editWidgetConfigElement );

        QVariantMap editWidgetConfiguration;

        const QDomNamedNodeMap configAttrs = ewv2CfgElem.attributes();
        for ( int configIndex = 0; configIndex < configAttrs.count(); ++configIndex )
        {
          const QDomAttr configAttr = configAttrs.item( configIndex ).toAttr();
          if ( configAttr.name() == "fieldEditable"_L1 )
          {
            editWidgetConfigElement.setAttribute( u"fieldEditable"_s, configAttr.value() );
          }
          else if ( configAttr.name() == "labelOnTop"_L1 )
          {
            editWidgetConfigElement.setAttribute( u"labelOnTop"_s, configAttr.value() );
          }
          else if ( configAttr.name() == "notNull"_L1 )
          {
            editWidgetConfigElement.setAttribute( u"notNull"_s, configAttr.value() );
          }
          else if ( configAttr.name() == "constraint"_L1 )
          {
            constraintExpressionElem.setAttribute( u"exp"_s, configAttr.value() );
          }
          else if ( configAttr.name() == "constraintDescription"_L1 )
          {
            constraintExpressionElem.setAttribute( u"desc"_s, configAttr.value() );
          }
          else
          {
            editWidgetConfiguration.insert( configAttr.name(), configAttr.value() );
          }
        }

        if ( ewv2Type == "ValueMap"_L1 )
        {
          const QDomNodeList configElements = ewv2CfgElem.childNodes();
          QVariantMap map;
          for ( int configIndex = 0; configIndex < configElements.count(); ++configIndex )
          {
            const QDomElement configElem = configElements.at( configIndex ).toElement();
            map.insert( configElem.attribute( u"key"_s ), configElem.attribute( u"value"_s ) );
          }
          editWidgetConfiguration.insert( u"map"_s, map );
        }
        else if ( ewv2Type == "Photo"_L1 )
        {
          editWidgetElement.setAttribute( u"type"_s, u"ExternalResource"_s );

          editWidgetConfiguration.insert( u"DocumentViewer"_s, 1 );
          editWidgetConfiguration.insert( u"DocumentViewerHeight"_s, editWidgetConfiguration.value( u"Height"_s ) );
          editWidgetConfiguration.insert( u"DocumentViewerWidth"_s, editWidgetConfiguration.value( u"Width"_s ) );
          editWidgetConfiguration.insert( u"RelativeStorage"_s, 1 );
        }
        else if ( ewv2Type == "FileName"_L1 )
        {
          editWidgetElement.setAttribute( u"type"_s, u"ExternalResource"_s );

          editWidgetConfiguration.insert( u"RelativeStorage"_s, 1 );
        }
        else if ( ewv2Type == "WebView"_L1 )
        {
          editWidgetElement.setAttribute( u"type"_s, u"ExternalResource"_s );

          editWidgetConfiguration.insert( u"DocumentViewerHeight"_s, editWidgetConfiguration.value( u"Height"_s ) );
          editWidgetConfiguration.insert( u"DocumentViewerWidth"_s, editWidgetConfiguration.value( u"Width"_s ) );
          editWidgetConfiguration.insert( u"RelativeStorage"_s, 1 );
        }

        editWidgetConfigElement.appendChild( QgsXmlUtils::writeVariant( editWidgetConfiguration, pft->dom() ) );
      }
    }
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


void transformContrastEnhancement( QDomDocument &doc, const QDomElement &rasterproperties, QDomElement &rendererElem )
{
  if ( rasterproperties.isNull() || rendererElem.isNull() )
  {
    return;
  }

  double minimumValue = 0;
  double maximumValue = 0;
  const QDomElement contrastMinMaxElem = rasterproperties.firstChildElement( u"contrastEnhancementMinMaxValues"_s );
  if ( contrastMinMaxElem.isNull() )
  {
    return;
  }

  const QDomElement contrastEnhancementAlgorithmElem = rasterproperties.firstChildElement( u"mContrastEnhancementAlgorithm"_s );
  if ( contrastEnhancementAlgorithmElem.isNull() )
  {
    return;
  }

  //convert enhancement name to enumeration
  int algorithmEnum = 0;
  const QString algorithmString = contrastEnhancementAlgorithmElem.text();
  if ( algorithmString == "StretchToMinimumMaximum"_L1 )
  {
    algorithmEnum = 1;
  }
  else if ( algorithmString == "StretchAndClipToMinimumMaximum"_L1 )
  {
    algorithmEnum = 2;
  }
  else if ( algorithmString == "ClipToMinimumMaximum"_L1 )
  {
    algorithmEnum = 3;
  }
  else if ( algorithmString == "UserDefinedEnhancement"_L1 )
  {
    algorithmEnum = 4;
  }

  const QDomNodeList minMaxEntryList = contrastMinMaxElem.elementsByTagName( u"minMaxEntry"_s );
  QStringList enhancementNameList;
  if ( minMaxEntryList.size() == 1 )
  {
    enhancementNameList << u"contrastEnhancement"_s;
  }
  if ( minMaxEntryList.size() == 3 )
  {
    enhancementNameList << u"redContrastEnhancement"_s << u"greenContrastEnhancement"_s << u"blueContrastEnhancement"_s;
  }
  if ( minMaxEntryList.size() > enhancementNameList.size() )
  {
    return;
  }

  QDomElement minMaxEntryElem;
  for ( int i = 0; i < minMaxEntryList.size(); ++i )
  {
    minMaxEntryElem = minMaxEntryList.at( i ).toElement();
    const QDomElement minElem = minMaxEntryElem.firstChildElement( u"min"_s );
    if ( minElem.isNull() )
    {
      return;
    }
    minimumValue = minElem.text().toDouble();

    const QDomElement maxElem = minMaxEntryElem.firstChildElement( u"max"_s );
    if ( maxElem.isNull() )
    {
      return;
    }
    maximumValue = maxElem.text().toDouble();

    QDomElement newContrastEnhancementElem = doc.createElement( enhancementNameList.at( i ) );
    QDomElement newMinValElem = doc.createElement( u"minValue"_s );
    const QDomText minText = doc.createTextNode( QString::number( minimumValue ) );
    newMinValElem.appendChild( minText );
    newContrastEnhancementElem.appendChild( newMinValElem );
    QDomElement newMaxValElem = doc.createElement( u"maxValue"_s );
    const QDomText maxText = doc.createTextNode( QString::number( maximumValue ) );
    newMaxValElem.appendChild( maxText );
    newContrastEnhancementElem.appendChild( newMaxValElem );

    QDomElement newAlgorithmElem = doc.createElement( u"algorithm"_s );
    const QDomText newAlgorithmText = doc.createTextNode( QString::number( algorithmEnum ) );
    newAlgorithmElem.appendChild( newAlgorithmText );
    newContrastEnhancementElem.appendChild( newAlgorithmElem );

    rendererElem.appendChild( newContrastEnhancementElem );
  }
}

void QgsProcessingModelFileTransform::fixOldSymbolLayerReferences( const QMap<QString, QgsMapLayer *> &mapLayers )
{
  for ( QgsMapLayer *ml : mapLayers )
  {
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( ml );
    if ( !vl )
      continue;

    auto migrateOldReferences = [&mapLayers]( const QList<QgsSymbolLayerReference> &slRefs ) {
      QList<QgsSymbolLayerReference> newRefs;
      for ( const QgsSymbolLayerReference &slRef : slRefs )
      {
        const QgsVectorLayer *vlRef = qobject_cast<QgsVectorLayer *>( mapLayers[slRef.layerId()] );
        const QgsFeatureRenderer *renderer = vlRef ? vlRef->renderer() : nullptr;
        Q_NOWARN_DEPRECATED_PUSH
        QSet<const QgsSymbolLayer *> symbolLayers = renderer ? QgsSymbolLayerUtils::toSymbolLayerPointers( renderer, QSet<QgsSymbolLayerId>() << slRef.symbolLayerId() ) : QSet<const QgsSymbolLayer *>();
        Q_NOWARN_DEPRECATED_POP
        const QString slId = symbolLayers.isEmpty() ? QString() : ( *symbolLayers.constBegin() )->id();
        newRefs << QgsSymbolLayerReference( slRef.layerId(), slId );
      }

      return newRefs;
    };

    if ( QgsAbstractVectorLayerLabeling *labeling = vl->labeling() )
    {
      const QStringList subProviders = labeling->subProviders();
      for ( const QString &provider : subProviders )
      {
        QgsPalLayerSettings settings = labeling->settings( provider );
        QgsTextFormat format = settings.format();
        QList<QgsSymbolLayerReference> newMaskedSymbolLayers = migrateOldReferences( format.mask().maskedSymbolLayers() );
        format.mask().setMaskedSymbolLayers( newMaskedSymbolLayers );
        settings.setFormat( format );
        labeling->setSettings( new QgsPalLayerSettings( settings ), provider );
      }
    }

    if ( QgsFeatureRenderer *renderer = vl->renderer() )
    {
      class SymbolLayerVisitor : public QgsStyleEntityVisitorInterface
      {
        public:
          bool visitEnter( const QgsStyleEntityVisitorInterface::Node &node ) override { return ( node.type == QgsStyleEntityVisitorInterface::NodeType::SymbolRule ); }

          void visitSymbol( const QgsSymbol *symbol )
          {
            for ( int idx = 0; idx < symbol->symbolLayerCount(); idx++ )
            {
              const QgsSymbolLayer *sl = symbol->symbolLayer( idx );

              // recurse over sub symbols
              const QgsSymbol *subSymbol = const_cast<QgsSymbolLayer *>( sl )->subSymbol();
              if ( subSymbol )
                visitSymbol( subSymbol );

              if ( const QgsMaskMarkerSymbolLayer *maskLayer = dynamic_cast<const QgsMaskMarkerSymbolLayer *>( sl ) )
                maskSymbolLayers << maskLayer;
            }
          }

          bool visit( const QgsStyleEntityVisitorInterface::StyleLeaf &leaf ) override
          {
            if ( leaf.entity && leaf.entity->type() == QgsStyle::SymbolEntity )
            {
              auto symbolEntity = static_cast<const QgsStyleSymbolEntity *>( leaf.entity );
              if ( symbolEntity->symbol() )
                visitSymbol( symbolEntity->symbol() );
            }
            return true;
          }

          QList<const QgsMaskMarkerSymbolLayer *> maskSymbolLayers;
      };

      SymbolLayerVisitor visitor;
      renderer->accept( &visitor );

      for ( const QgsMaskMarkerSymbolLayer *maskSymbolLayer : std::as_const( visitor.maskSymbolLayers ) )
        // Ugly but there is no other proper way to get those layer in order to modify them
        const_cast<QgsMaskMarkerSymbolLayer *>( maskSymbolLayer )->setMasks( migrateOldReferences( maskSymbolLayer->masks() ) );
    }
  }
}
