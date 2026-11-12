/***************************************************************************
                          QgsProcessingModelFileTransform.h
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


#ifndef QGSPROCESSINGMODELFILETRANSFORM_H
#define QGSPROCESSINGMODELFILETRANSFORM_H


#include "qgis_core.h"
#include "qgsprojectversion.h"

#include <QDomDocument>
#include <QString>

class QgsRasterLayer;
class QgsMapLayer;

/**
 * \ingroup core
 * \brief Convert from older project file versions to newer.
 *
 * This class provides possibility to store a project file as a QDomDocument,
 * and provides the ability to specify version of the project file, and
 * perform upgrades to a more recent version
 */
class CORE_EXPORT QgsProcessingModelFileTransform
{
  public:
    //Default constructor
    //QgsProcessingModelFileTransform() {}

    /**
     * Create an instance from a Dom and a supplied version
     * \param domDocument The Dom document to use as content
     * \param version Version number
     */
    QgsProcessingModelFileTransform( QDomDocument &domDocument, const QgsProjectVersion &version )
    {
      mDom = domDocument;
      mCurrentVersion = version;
    }


    bool updateRevision( const QgsProjectVersion &version );

    /**
     * Prints the contents to the console.
     */
    void dump();

    /**
     * The current dom document
     *
     * \since QGIS 3.12
     */
    QDomDocument &dom();

    /**
     * The current project version
     *
     * \since QGIS 3.12
     */
    QgsProjectVersion currentVersion() const;

    /**
     * QgsSymbolLayerReference uses QgsSymbolLayer unique uuid identifier since QGIS 3.30, instead of the symbol
     * key (rule for QgsRuleBasedRenderer for instance) and index path, so this method migrates \a mapLayers old references
     * to new ones.
     * \since QGIS 3.30
     * \deprecated QGIS 3.30
     */
    Q_DECL_DEPRECATED static void fixOldSymbolLayerReferences( const QMap<QString, QgsMapLayer *> &mapLayers );

  private:
    QDomDocument mDom;
    QgsProjectVersion mCurrentVersion;
};


#endif //QGSPROCESSINGMODELFILETRANSFORM_H
