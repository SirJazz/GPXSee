#ifndef GARMINCOLORS_H
#define GARMINCOLORS_H

#include <QColor>
#include <QString>
#include <QStringList>

/* The fixed set of track/route colors Garmin devices and BaseCamp use in the
   gpxx:DisplayColor extension element. */
namespace GarminColors
{
	const QStringList &names();
	QColor color(const QString &name);
	bool isGarminName(const QString &name);
	QString canonicalName(const QString &name);
	QString nearestName(const QColor &color);
}

#endif // GARMINCOLORS_H
