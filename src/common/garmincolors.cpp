#include <QHash>
#include "garmincolors.h"

struct GarminColor {
	const char *name;
	QRgb rgb;
};

/* The 16 classic VGA colors, as used by BaseCamp and the devices. */
static const GarminColor COLORS[] = {
	{"Black", 0x000000},
	{"DarkRed", 0x800000},
	{"DarkGreen", 0x008000},
	{"DarkYellow", 0x808000},
	{"DarkBlue", 0x000080},
	{"DarkMagenta", 0x800080},
	{"DarkCyan", 0x008080},
	{"LightGray", 0xc0c0c0},
	{"DarkGray", 0x808080},
	{"Red", 0xff0000},
	{"Green", 0x00ff00},
	{"Yellow", 0xffff00},
	{"Blue", 0x0000ff},
	{"Magenta", 0xff00ff},
	{"Cyan", 0x00ffff},
	{"White", 0xffffff},
	{"Transparent", 0x000000}
};

static const int COLOR_COUNT = sizeof(COLORS) / sizeof(COLORS[0]);

const QStringList &GarminColors::names()
{
	static QStringList list;

	if (list.isEmpty())
		for (int i = 0; i < COLOR_COUNT; i++)
			list.append(QString::fromLatin1(COLORS[i].name));

	return list;
}

static int indexOf(const QString &name)
{
	QString n(name.trimmed());

	for (int i = 0; i < COLOR_COUNT; i++)
		if (!n.compare(QLatin1String(COLORS[i].name), Qt::CaseInsensitive))
			return i;

	return -1;
}

bool GarminColors::isGarminName(const QString &name)
{
	return (indexOf(name) >= 0);
}

QString GarminColors::canonicalName(const QString &name)
{
	int i = indexOf(name);
	return (i < 0) ? QString() : QString::fromLatin1(COLORS[i].name);
}

QColor GarminColors::color(const QString &name)
{
	int i = indexOf(name);

	if (i < 0)
		return QColor(name.trimmed());
	if (!qstrcmp(COLORS[i].name, "Transparent"))
		return QColor(Qt::transparent);

	return QColor(COLORS[i].rgb);
}

QString GarminColors::nearestName(const QColor &color)
{
	if (!color.isValid())
		return QString();

	int best = 0;
	long bestDist = -1;

	for (int i = 0; i < COLOR_COUNT; i++) {
		if (!qstrcmp(COLORS[i].name, "Transparent"))
			continue;
		QColor c(COLORS[i].rgb);
		long dr = c.red() - color.red();
		long dg = c.green() - color.green();
		long db = c.blue() - color.blue();
		long dist = dr * dr + dg * dg + db * db;
		if (bestDist < 0 || dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}

	return QString::fromLatin1(COLORS[best].name);
}
