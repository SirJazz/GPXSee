#include <QAbstractTableModel>
#include <QComboBox>
#include <QTableView>
#include <QHeaderView>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QGridLayout>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QLocale>
#include <QPainter>
#include <QMessageBox>
#include <QCoreApplication>
#include <cmath>
#include "common/garmincolors.h"
#include "data/gpxcolorwriter.h"
#include "trackdialog.h"

static qreal bearing(const Coordinates &a, const Coordinates &b)
{
	qreal lat1 = deg2rad(a.lat()), lat2 = deg2rad(b.lat());
	qreal dLon = deg2rad(b.lon() - a.lon());
	qreal y = sin(dLon) * cos(lat2);
	qreal x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);
	qreal deg = rad2deg(atan2(y, x));
	return (deg < 0) ? deg + 360.0 : deg;
}

static QString colorLabel(const QString &name)
{
	static const char *labels[][2] = {
		{"Black", QT_TRANSLATE_NOOP("TrackDialog", "Black")},
		{"DarkRed", QT_TRANSLATE_NOOP("TrackDialog", "Dark red")},
		{"DarkGreen", QT_TRANSLATE_NOOP("TrackDialog", "Dark green")},
		{"DarkYellow", QT_TRANSLATE_NOOP("TrackDialog", "Dark yellow")},
		{"DarkBlue", QT_TRANSLATE_NOOP("TrackDialog", "Dark blue")},
		{"DarkMagenta", QT_TRANSLATE_NOOP("TrackDialog", "Dark magenta")},
		{"DarkCyan", QT_TRANSLATE_NOOP("TrackDialog", "Dark cyan")},
		{"LightGray", QT_TRANSLATE_NOOP("TrackDialog", "Light gray")},
		{"DarkGray", QT_TRANSLATE_NOOP("TrackDialog", "Dark gray")},
		{"Red", QT_TRANSLATE_NOOP("TrackDialog", "Red")},
		{"Green", QT_TRANSLATE_NOOP("TrackDialog", "Green")},
		{"Yellow", QT_TRANSLATE_NOOP("TrackDialog", "Yellow")},
		{"Blue", QT_TRANSLATE_NOOP("TrackDialog", "Blue")},
		{"Magenta", QT_TRANSLATE_NOOP("TrackDialog", "Magenta")},
		{"Cyan", QT_TRANSLATE_NOOP("TrackDialog", "Cyan")},
		{"White", QT_TRANSLATE_NOOP("TrackDialog", "White")},
		{"Transparent", QT_TRANSLATE_NOOP("TrackDialog", "Transparent")}
	};

	for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++)
		if (name == QLatin1String(labels[i][0]))
			return QCoreApplication::translate("TrackDialog", labels[i][1]);

	return name;
}

static QIcon colorIcon(const QColor &color)
{
	QPixmap pm(16, 16);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setPen(QPen(QColor(0, 0, 0, 120), 1));
	if (color.alpha() == 0) {
		p.setBrush(Qt::white);
		p.drawRect(QRectF(1.5, 1.5, 13, 13));
		p.setPen(QPen(Qt::red, 1));
		p.drawLine(QPointF(2, 14), QPointF(14, 2));
	} else {
		p.setBrush(color);
		p.drawRect(QRectF(1.5, 1.5, 13, 13));
	}

	return QIcon(pm);
}

class TrackPointModel : public QAbstractTableModel
{
public:
	struct Row {
		const Trackpoint *point;
		int segment;
		qreal leg;
		qreal legTime;
		qreal speed;
		qreal course;
	};

	enum Column {Index, Elevation, Leg, LegTime, Speed, Course, Time,
	  Position, ColumnCount};

	TrackPointModel(const TrackData &data, Units units,
	  CoordinatesFormat format, const QTimeZone &zone, QObject *parent)
	  : QAbstractTableModel(parent), _units(units), _format(format),
	  _zone(zone)
	{
		for (int i = 0; i < data.size(); i++) {
			const SegmentData &sd = data.at(i);
			for (int j = 0; j < sd.size(); j++) {
				Row r;
				r.point = &sd.at(j);
				r.segment = i;
				r.leg = 0;
				r.legTime = NAN;
				r.speed = NAN;
				r.course = NAN;
				if (j) {
					const Trackpoint &prev = sd.at(j - 1);
					r.leg = prev.coordinates().distanceTo(
					  r.point->coordinates());
					r.course = bearing(prev.coordinates(),
					  r.point->coordinates());
					if (prev.hasTimestamp() && r.point->hasTimestamp()) {
						r.legTime = prev.timestamp().msecsTo(
						  r.point->timestamp()) / 1000.0;
						if (r.legTime > 0)
							r.speed = r.leg / r.legTime;
					}
				}
				_rows.append(r);
			}
		}
	}

	int rowCount(const QModelIndex &parent = QModelIndex()) const
	  {return parent.isValid() ? 0 : _rows.size();}
	int columnCount(const QModelIndex &parent = QModelIndex()) const
	  {return parent.isValid() ? 0 : ColumnCount;}

	const Row &row(int i) const {return _rows.at(i);}

	QVariant headerData(int section, Qt::Orientation orientation,
	  int role) const
	{
		if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
			return QVariant();

		switch (section) {
			case Index:
				return TrackDialog::tr("Index");
			case Elevation:
				return TrackDialog::tr("Elevation");
			case Leg:
				return TrackDialog::tr("Leg distance");
			case LegTime:
				return TrackDialog::tr("Leg time");
			case Speed:
				return TrackDialog::tr("Leg speed");
			case Course:
				return TrackDialog::tr("Course");
			case Time:
				return TrackDialog::tr("Time");
			case Position:
				return TrackDialog::tr("Position");
		}

		return QVariant();
	}

	QVariant data(const QModelIndex &index, int role) const
	{
		if (!index.isValid())
			return QVariant();
		if (role == Qt::TextAlignmentRole)
			return (index.column() == Position || index.column() == Time)
			  ? QVariant(Qt::AlignLeft | Qt::AlignVCenter)
			  : QVariant(Qt::AlignRight | Qt::AlignVCenter);
		if (role != Qt::DisplayRole)
			return QVariant();

		const Row &r = _rows.at(index.row());
		QLocale l;

		switch (index.column()) {
			case Index:
				return index.row() + 1;
			case Elevation:
				return r.point->hasElevation()
				  ? Format::elevation(r.point->elevation(), _units) : QString();
			case Leg:
				return Format::distance(r.leg, _units);
			case LegTime:
				return std::isnan(r.legTime) ? QString()
				  : Format::timeSpan(r.legTime);
			case Speed:
				return std::isnan(r.speed) ? QString()
				  : speedString(r.speed);
			case Course:
				return std::isnan(r.course) ? QString()
				  : l.toString(r.course, 'f', 1) + QChar(0x00B0);
			case Time:
				return r.point->hasTimestamp()
				  ? l.toString(r.point->timestamp().toTimeZone(_zone),
				  QLocale::ShortFormat) : QString();
			case Position:
				return Format::coordinates(r.point->coordinates(), _format);
		}

		return QVariant();
	}

	QString speedString(qreal ms) const
	{
		QLocale l;
		if (_units == Imperial)
			return l.toString(ms * MS2MIH, 'f', 0) + UNIT_SPACE
			  + TrackDialog::tr("mi/h");
		else if (_units == Nautical)
			return l.toString(ms * MS2KN, 'f', 0) + UNIT_SPACE
			  + TrackDialog::tr("kn");
		else
			return l.toString(ms * MS2KMH, 'f', 0) + UNIT_SPACE
			  + TrackDialog::tr("km/h");
	}

private:
	QVector<Row> _rows;
	Units _units;
	CoordinatesFormat _format;
	QTimeZone _zone;
};

QString TrackDialog::speed(qreal ms) const
{
	return _model->speedString(ms);
}

QGroupBox *TrackDialog::summaryBox(const QString &title,
  const QList<QPair<QString, QString> > &rows)
{
	QGroupBox *box = new QGroupBox(title);
	QFormLayout *layout = new QFormLayout(box);
	layout->setLabelAlignment(Qt::AlignRight);
	for (int i = 0; i < rows.size(); i++) {
		QLabel *value = new QLabel(rows.at(i).second);
		value->setTextInteractionFlags(Qt::TextSelectableByMouse);
		layout->addRow(rows.at(i).first + ":", value);
	}

	return box;
}

TrackDialog::TrackDialog(const Track &track, const QString &file,
  int occurrence, Map *map, Units units, CoordinatesFormat format,
  const QTimeZone &zone, QWidget *parent)
  : QDialog(parent), _track(track), _file(file), _occurrence(occurrence),
  _units(units), _saved(false)
{
	QString title(track.name().isEmpty()
	  ? QFileInfo(file).completeBaseName() : track.name());
	setWindowTitle(title);

	_model = new TrackPointModel(_track.data(), units, format, zone, this);

	/* Name and color */
	QLineEdit *name = new QLineEdit(title);
	name->setReadOnly(true);

	_color = new QComboBox();
	const QStringList &names = GarminColors::names();
	for (int i = 0; i < names.size(); i++)
		_color->addItem(colorIcon(GarminColors::color(names.at(i))),
		  colorLabel(names.at(i)), names.at(i));
	QColor current(track.style().color());
	_originalColor = current.isValid()
	  ? GarminColors::nearestName(current) : QString();
	if (current.isValid() && current.alpha() == 0)
		_originalColor = "Transparent";
	if (_originalColor.isEmpty())
		_color->setCurrentIndex(-1);
	else
		_color->setCurrentIndex(names.indexOf(_originalColor));
	_color->setToolTip(tr("Garmin display color (gpxx:DisplayColor)"));

	_saveButton = new QPushButton(tr("Save color to file"));
	_saveButton->setEnabled(false);
	bool gpx = !QFileInfo(file).suffix().compare("gpx", Qt::CaseInsensitive);
	if (!gpx)
		_color->setEnabled(false);

	QHBoxLayout *nameLayout = new QHBoxLayout();
	nameLayout->addWidget(name, 1);
	nameLayout->addWidget(_color);
	nameLayout->addWidget(_saveButton);

	_status = new QLabel(gpx ? QString(QFileInfo(file).fileName())
	  : tr("Colors can only be saved to GPX files."));
	_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

	/* Summary */
	int points = 0;
	for (int i = 0; i < _track.data().size(); i++)
		points += _track.data().at(i).size();

	qreal dist = _track.distance();
	qreal time = _track.time();
	qreal moving = _track.movingTime();

	qreal maxSpeed = NAN;
	GraphPair sp(_track.speed());
	for (int i = 0; i < sp.primary().size(); i++)
		for (int j = 0; j < sp.primary().at(i).size(); j++) {
			qreal v = sp.primary().at(i).at(j).y();
			if (std::isnan(maxSpeed) || v > maxSpeed)
				maxSpeed = v;
		}

	qreal minEle = NAN, maxEle = NAN, ascent = 0, descent = 0;
	GraphPair ele(_track.elevation(map));
	for (int i = 0; i < ele.primary().size(); i++) {
		const GraphSegment &seg = ele.primary().at(i);
		for (int j = 0; j < seg.size(); j++) {
			qreal y = seg.at(j).y();
			if (std::isnan(minEle) || y < minEle)
				minEle = y;
			if (std::isnan(maxEle) || y > maxEle)
				maxEle = y;
			if (j) {
				qreal prev = seg.at(j - 1).y();
				if (y > prev)
					ascent += y - prev;
				else
					descent += prev - y;
			}
		}
	}

	QLocale l;
	QString none("-");
	QList<QPair<QString, QString> > overview, timeRows, speedRows, eleRows;
	overview << qMakePair(tr("Points"), l.toString(points))
	  << qMakePair(tr("Distance"), Format::distance(dist, units));
	timeRows << qMakePair(tr("Total time"), time > 0
	    ? Format::timeSpan(time) : none)
	  << qMakePair(tr("Moving time"), moving > 0
	    ? Format::timeSpan(moving) : none)
	  << qMakePair(tr("Pause time"), (time > 0 && moving > 0)
	    ? Format::timeSpan(time - moving) : none);
	speedRows << qMakePair(tr("Average"), time > 0 ? speed(dist / time) : none)
	  << qMakePair(tr("Moving average"), moving > 0
	    ? speed(dist / moving) : none)
	  << qMakePair(tr("Maximum"), std::isnan(maxSpeed)
	    ? none : speed(maxSpeed));
	eleRows << qMakePair(tr("Minimum"), std::isnan(minEle)
	    ? none : Format::elevation(minEle, units))
	  << qMakePair(tr("Maximum"), std::isnan(maxEle)
	    ? none : Format::elevation(maxEle, units))
	  << qMakePair(tr("Ascent"), std::isnan(minEle)
	    ? none : Format::elevation(ascent, units))
	  << qMakePair(tr("Descent"), std::isnan(minEle)
	    ? none : Format::elevation(descent, units));

	QHBoxLayout *summary = new QHBoxLayout();
	summary->addWidget(summaryBox(tr("Overview"), overview));
	summary->addWidget(summaryBox(tr("Time"), timeRows));
	summary->addWidget(summaryBox(tr("Speed"), speedRows));
	summary->addWidget(summaryBox(tr("Elevation"), eleRows));

	/* Points */
	_points = new QTableView();
	_points->setModel(_model);
	_points->setSelectionBehavior(QAbstractItemView::SelectRows);
	_points->setSelectionMode(QAbstractItemView::SingleSelection);
	_points->verticalHeader()->hide();
	_points->verticalHeader()->setDefaultSectionSize(
	  _points->fontMetrics().height() + 4);
	_points->horizontalHeader()->setSectionResizeMode(
	  QHeaderView::ResizeToContents);
	_points->horizontalHeader()->setStretchLastSection(true);
	_points->setAlternatingRowColors(true);

	_center = new QCheckBox(tr("Center map on selected point"));
	_center->setChecked(true);

	QPushButton *close = new QPushButton(tr("Close"));
	QHBoxLayout *bottom = new QHBoxLayout();
	bottom->addWidget(_center);
	bottom->addStretch();
	bottom->addWidget(close);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(nameLayout);
	layout->addWidget(_status);
	layout->addLayout(summary);
	layout->addWidget(_points, 1);
	layout->addLayout(bottom);

	connect(_color, &QComboBox::currentIndexChanged, this,
	  &TrackDialog::colorChanged);
	connect(_saveButton, &QPushButton::clicked, this, &TrackDialog::saveColor);
	connect(_points->selectionModel(), &QItemSelectionModel::currentRowChanged,
	  this, &TrackDialog::rowChanged);
	connect(close, &QPushButton::clicked, this, &QDialog::accept);

	resize(900, 650);
}

void TrackDialog::colorChanged(int index)
{
	QString name(index >= 0 ? _color->itemData(index).toString() : QString());
	_saveButton->setEnabled(!name.isEmpty() && name != _originalColor);
}

void TrackDialog::saveColor()
{
	QString color(_color->currentData().toString());
	QString error;

	if (!GPXColorWriter::setTrackColor(_file, _track.name(), _occurrence,
	  color, error)) {
		QMessageBox::critical(this, windowTitle(),
		  tr("Error saving the color:") + "\n" + error);
		return;
	}

	_originalColor = color;
	_saved = true;
	_saveButton->setEnabled(false);
	_status->setText(tr("Color %1 saved to %2 (backup: %3)").arg(
	  colorLabel(color),
	  QFileInfo(_file).fileName(), QFileInfo(_file).fileName() + ".bak"));
}

void TrackDialog::rowChanged()
{
	if (!_center->isChecked())
		return;
	QModelIndex index(_points->currentIndex());
	if (!index.isValid())
		return;

	emit pointSelected(_model->row(index.row()).point->coordinates());
}
