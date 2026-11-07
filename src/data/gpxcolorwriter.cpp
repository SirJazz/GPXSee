#include <QFile>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QCoreApplication>
#include "common/garmincolors.h"
#include "gpxcolorwriter.h"

#define GPXX_NS "http://www.garmin.com/xmlschemas/GpxExtensions/v3"

static QString tr(const char *text)
{
	return QCoreApplication::translate("GPXColorWriter", text);
}

namespace {

struct Edit {
	Edit() : pos(-1), len(0) {}
	Edit(int pos, int len, const QString &text)
	  : pos(pos), len(len), text(text) {}

	int pos;
	int len;
	QString text;
};

/* Position of the '<' that starts the tag ending at offset end. */
int tagStart(const QString &content, qint64 end)
{
	return content.lastIndexOf(QLatin1Char('<'), (int)end - 1);
}

bool isEmptyElement(const QString &content, qint64 startTagEnd)
{
	return (startTagEnd >= 2
	  && content.mid((int)startTagEnd - 2, 2) == QLatin1String("/>"));
}

QString qualified(const QString &prefix, const char *name)
{
	return prefix.isEmpty() ? QString::fromLatin1(name)
	  : prefix + QLatin1Char(':') + QLatin1String(name);
}

QString colorElement(const QString &prefix, const QString &color)
{
	return QLatin1Char('<') + qualified(prefix, "DisplayColor")
	  + QLatin1Char('>') + color + QLatin1String("</")
	  + qualified(prefix, "DisplayColor") + QLatin1Char('>');
}

QString trackExtensionElement(const QString &prefix, const QString &color)
{
	return QLatin1Char('<') + qualified(prefix, "TrackExtension")
	  + QLatin1Char('>') + colorElement(prefix, color) + QLatin1String("</")
	  + qualified(prefix, "TrackExtension") + QLatin1Char('>');
}

/* Replaces an empty element tag (<x/>) ending at startTagEnd with
   <x>inner</x>. */
Edit expandEmpty(const QString &content, qint64 startTagEnd,
  const QString &qname, const QString &inner)
{
	int start = tagStart(content, startTagEnd);
	QString tag(content.mid(start, (int)startTagEnd - start));
	tag.chop(2);
	while (tag.endsWith(QLatin1Char(' ')) || tag.endsWith(QLatin1Char('\t'))
	  || tag.endsWith(QLatin1Char('\n')) || tag.endsWith(QLatin1Char('\r')))
		tag.chop(1);

	return Edit(start, (int)startTagEnd - start, tag + QLatin1Char('>')
	  + inner + QLatin1String("</") + qname + QLatin1Char('>'));
}

bool checkEncoding(const QString &content, QString &error)
{
	static const QRegularExpression re(
	  "^\\s*<\\?xml[^>]*encoding\\s*=\\s*[\"']([^\"']+)[\"']");
	QRegularExpressionMatch match(re.match(content));

	if (match.hasMatch()) {
		QString enc(match.captured(1).toLower());
		if (enc != QLatin1String("utf-8") && enc != QLatin1String("utf8")) {
			error = tr("Unsupported file encoding: %1").arg(match.captured(1));
			return false;
		}
	}

	return true;
}

bool wellFormed(const QString &content, QString &error)
{
	QXmlStreamReader reader(content);

	while (!reader.atEnd())
		reader.readNext();
	if (reader.hasError()) {
		error = reader.errorString();
		return false;
	}

	return true;
}

}

bool GPXColorWriter::setContentTrackColor(QString &content,
  const QString &trackName, int occurrence, const QString &garminColor, QString &error)
{
	QString color(GarminColors::canonicalName(garminColor));
	if (color.isEmpty()) {
		error = tr("Invalid color: %1").arg(garminColor);
		return false;
	}
	if (!checkEncoding(content, error))
		return false;

	QXmlStreamReader reader(content);

	/* Root element */
	if (!reader.readNextStartElement()
	  || reader.name() != QLatin1String("gpx")) {
		error = tr("Not a GPX file") + "";
		return false;
	}
	qint64 rootEnd = reader.characterOffset();
	QString gpxxPrefix;
	bool gpxxDeclared = false;
	QStringList usedPrefixes;
	const QXmlStreamNamespaceDeclarations ns(reader.namespaceDeclarations());
	for (int i = 0; i < ns.size(); i++) {
		usedPrefixes.append(ns.at(i).prefix().toString());
		if (ns.at(i).namespaceUri() == QLatin1String(GPXX_NS)) {
			gpxxPrefix = ns.at(i).prefix().toString();
			gpxxDeclared = true;
		}
	}

	int found = -1;
	Edit edit;
	QString trkPrefix;

	while (reader.readNextStartElement()) {
		if (reader.name() != QLatin1String("trk")) {
			reader.skipCurrentElement();
			continue;
		}

		trkPrefix = reader.prefix().toString();
		qint64 trkStartEnd = reader.characterOffset();
		QString name;
		qint64 firstSegStart = -1, extStartEnd = -1;
		bool extEmpty = false;
		QString extQName;
		qint64 teStartEnd = -1;
		bool teEmpty = false;
		QString tePrefix, teQName;
		qint64 colorTextStart = -1, colorTextEnd = -1;
		bool colorEmpty = false;
		QString colorQName;
		qint64 trkEndStart = -1;

		if (isEmptyElement(content, trkStartEnd)) {
			reader.readNext();
			continue;
		}

		while (reader.readNextStartElement()) {
			if (reader.name() == QLatin1String("name"))
				name = reader.readElementText();
			else if (reader.name() == QLatin1String("trkseg")) {
				if (firstSegStart < 0)
					firstSegStart = tagStart(content, reader.characterOffset());
				reader.skipCurrentElement();
			} else if (reader.name() == QLatin1String("extensions")
			  && extStartEnd < 0) {
				extStartEnd = reader.characterOffset();
				extQName = reader.qualifiedName().toString();
				extEmpty = isEmptyElement(content, extStartEnd);
				while (reader.readNextStartElement()) {
					if (reader.name() == QLatin1String("TrackExtension")
					  && reader.namespaceUri() == QLatin1String(GPXX_NS)
					  && teStartEnd < 0) {
						teStartEnd = reader.characterOffset();
						tePrefix = reader.prefix().toString();
						teQName = reader.qualifiedName().toString();
						teEmpty = isEmptyElement(content, teStartEnd);
						while (reader.readNextStartElement()) {
							if (reader.name() == QLatin1String("DisplayColor")
							  && colorTextStart < 0) {
								colorTextStart = reader.characterOffset();
								colorQName = reader.qualifiedName().toString();
								colorEmpty = isEmptyElement(content,
								  colorTextStart);
								reader.readElementText();
								colorTextEnd = colorEmpty ? colorTextStart
								  : tagStart(content, reader.characterOffset());
							} else
								reader.skipCurrentElement();
						}
					} else
						reader.skipCurrentElement();
				}
			} else
				reader.skipCurrentElement();
		}
		if (reader.hasError())
			break;
		trkEndStart = tagStart(content, reader.characterOffset());

		if (name != trackName)
			continue;
		if (++found != occurrence)
			continue;

		if (colorTextStart >= 0) {
			if (colorEmpty)
				edit = expandEmpty(content, colorTextStart, colorQName, color);
			else
				edit = Edit((int)colorTextStart,
				  (int)(colorTextEnd - colorTextStart), color);
		} else if (teStartEnd >= 0) {
			if (teEmpty)
				edit = expandEmpty(content, teStartEnd, teQName,
				  colorElement(tePrefix, color));
			else
				edit = Edit((int)teStartEnd, 0, colorElement(tePrefix, color));
		} else {
			if (!gpxxDeclared) {
				gpxxPrefix = "gpxx";
				for (int i = 2; usedPrefixes.contains(gpxxPrefix); i++)
					gpxxPrefix = QString("gpxx%1").arg(i);
			}
			QString te(trackExtensionElement(gpxxPrefix, color));

			if (extStartEnd >= 0) {
				if (extEmpty)
					edit = expandEmpty(content, extStartEnd, extQName, te);
				else
					edit = Edit((int)extStartEnd, 0, te);
			} else {
				QString ext(QLatin1Char('<') + qualified(trkPrefix,
				  "extensions") + QLatin1Char('>') + te + QLatin1String("</")
				  + qualified(trkPrefix, "extensions") + QLatin1Char('>'));
				edit = Edit((int)(firstSegStart >= 0
				  ? firstSegStart : trkEndStart), 0, ext);
			}
		}
		break;
	}

	if (reader.hasError()) {
		error = reader.errorString();
		return false;
	}
	if (edit.pos < 0) {
		error = tr("Track not found: %1").arg(trackName);
		return false;
	}

	QString result(content);
	result.replace(edit.pos, edit.len, edit.text);

	/* The namespace declaration is added to the root element. The root
	   element precedes the track, so the edit position is not affected. */
	if (!gpxxDeclared && edit.text.contains(qualified(gpxxPrefix,
	  "TrackExtension"))) {
		int pos = (int)rootEnd - 1;
		if (isEmptyElement(content, rootEnd))
			pos--;
		result.insert(pos, QString(" xmlns:%1=\"%2\"").arg(gpxxPrefix,
		  GPXX_NS));
	}

	if (!wellFormed(result, error))
		return false;

	content = result;
	return true;
}

bool GPXColorWriter::setTrackColor(const QString &fileName,
  const QString &trackName, int occurrence, const QString &garminColor,
  QString &error)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		error = file.errorString();
		return false;
	}
	QByteArray data(file.readAll());
	file.close();

	bool bom = data.startsWith("\xEF\xBB\xBF");
	QString content(QString::fromUtf8(bom ? data.mid(3) : data));

	if (!setContentTrackColor(content, trackName, occurrence, garminColor,
	  error))
		return false;

	QString backup(fileName + ".bak");
	if (QFile::exists(backup) && !QFile::remove(backup)) {
		error = tr("Can not remove the old backup file: %1").arg(backup);
		return false;
	}
	if (!QFile::copy(fileName, backup)) {
		error = tr("Can not create the backup file: %1").arg(backup);
		return false;
	}

	QSaveFile out(fileName);
	if (!out.open(QIODevice::WriteOnly)) {
		error = out.errorString();
		return false;
	}
	if (bom)
		out.write("\xEF\xBB\xBF");
	out.write(content.toUtf8());
	if (!out.commit()) {
		error = out.errorString();
		return false;
	}

	return true;
}
