#ifndef GPXCOLORWRITER_H
#define GPXCOLORWRITER_H

#include <QString>

/* Sets the Garmin display color (gpxx:TrackExtension/gpxx:DisplayColor) of
   a single <trk> in an existing GPX file. The file is modified in place with
   a minimal textual edit, everything else in the file is left untouched.
   A backup copy (<file>.bak) is created before the file is overwritten. */
namespace GPXColorWriter
{
	/* The track is identified by its name and the occurrence index among
	   the tracks with the same name (0 = first). */
	bool setTrackColor(const QString &fileName, const QString &trackName,
	  int occurrence, const QString &garminColor, QString &error);

	/* Same as above but operates on the file content only (no I/O).
	   Exposed for testing. */
	bool setContentTrackColor(QString &content, const QString &trackName,
	  int occurrence, const QString &garminColor, QString &error);
}

#endif // GPXCOLORWRITER_H
