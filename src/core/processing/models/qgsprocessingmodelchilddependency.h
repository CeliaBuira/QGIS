/***************************************************************************
                         qgsprocessingmodelchilddependency.h
                         -----------------------------
    begin                : April 2020
    copyright            : (C) 2020 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSPROCESSINGMODELCHILDDEPENDENCY_H
#define QGSPROCESSINGMODELCHILDDEPENDENCY_H

#include "qgis.h"
#include "qgis_core.h"

#include <QString>

using namespace Qt::StringLiterals;

///@cond NOT_STABLE

/**
 * \class QgsProcessingModelChildDependency
 * \ingroup core
 * \brief Contains details of a child algorithm dependency.
 * \since QGIS 3.14
 */
class CORE_EXPORT QgsProcessingModelChildDependency
{
  public:
    /**
     * Constructor for QgsProcessingModelChildDependency, with the specified \a childId.
     */
    QgsProcessingModelChildDependency( const QString &fromChildId = QString(), const QString &conditionalBranch = QString() )
      : fromChildId( fromChildId )
      , conditionalBranch( conditionalBranch )
    {}

    //! Child algorithm ID
    QString fromChildId;

    //! Target child algorithm ID
    QString toChildId;

    //! Conditional branch output name, if applicable.
    QString conditionalBranch;

    /**
     * Saves this dependency to a QVariant.
     * \see loadVariant()
     */
    QVariant toVariant() const
    {
      QVariantMap res;
      res.insert( u"from_child_id"_s, fromChildId );
      res.insert( u"to_child_id"_s, toChildId );
      res.insert( u"child_id"_s, fromChildId ); // Keep to remain compatible with older versions of QGIS
      res.insert( u"conditional_branch"_s, conditionalBranch );
      return res;
    }

    /**
     * Loads this dependency from a QVariantMap.
     * \see toVariant()
     */
    bool loadVariant( const QVariantMap &map )
    {
      fromChildId = map.value( u"from_child_id"_s ).toString();
      if ( fromChildId.isEmpty() )
      {
        // Try to load from the former "child_id" key if "from_child_id" is not present.
        fromChildId = map.value( u"child_id"_s ).toString();
      }

      toChildId = map.value( u"to_child_id"_s ).toString();
      conditionalBranch = map.value( u"conditional_branch"_s ).toString();
      return true;
    }

    // TODO c++20 - replace with = default
    bool operator==( const QgsProcessingModelChildDependency &other ) const { return fromChildId == other.fromChildId && toChildId == other.toChildId && conditionalBranch == other.conditionalBranch; }

    bool operator!=( const QgsProcessingModelChildDependency &other ) const { return !( *this == other ); }

    // bool operator<( const QgsProcessingModelChildDependency &other ) const { return toChildId == other.toChildId ? conditionalBranch < other.conditionalBranch : toChildId < other.toChildId; }
};

Q_DECLARE_METATYPE( QgsProcessingModelChildDependency )

///@endcond

#endif // QGSPROCESSINGMODELCHILDDEPENDENCY_H
