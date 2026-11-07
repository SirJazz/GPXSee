#ifndef TRACKDIALOG_H
#define TRACKDIALOG_H

#include <QDialog>
#include <QTimeZone>
#include "data/track.h"
#include "units.h"
#include "format.h"

class QComboBox;
class QTableView;
class QCheckBox;
class QPushButton;
class QLabel;
class QGroupBox;
class Map;
class TrackPointModel;
class Coordinates;

/* Track properties dialog: summary, list of all track points and the
   Garmin display color that can be saved back to the GPX file. */
class TrackDialog : public QDialog
{
	Q_OBJECT

public:
	TrackDialog(const Track &track, const QString &file, int occurrence,
	  Map *map, Units units, CoordinatesFormat format,
	  const QTimeZone &zone, QWidget *parent = 0);

	bool saved() const {return _saved;}

signals:
	void pointSelected(const Coordinates &c);

private slots:
	void colorChanged(int index);
	void saveColor();
	void rowChanged();

private:
	QGroupBox *summaryBox(const QString &title,
	  const QList<QPair<QString, QString> > &rows);
	QString speed(qreal ms) const;

	Track _track;
	QString _file;
	int _occurrence;
	Units _units;
	QString _originalColor;
	bool _saved;

	QComboBox *_color;
	QPushButton *_saveButton;
	QLabel *_status;
	QTableView *_points;
	TrackPointModel *_model;
	QCheckBox *_center;
};

#endif // TRACKDIALOG_H
