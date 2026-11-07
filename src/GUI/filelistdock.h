#ifndef FILELISTDOCK_H
#define FILELISTDOCK_H

#include <QDockWidget>
#include <QList>
#include <QHash>
#include <QPair>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QPushButton;
class PathItem;
class Track;
class Data;

/* Side panel listing all loaded tracks and routes (one row per path, like
   the BaseCamp data list). Each row can be shown/hidden on the map, a
   click selects and zooms to the path, a double click opens the
   properties dialog. */
class FileListDock : public QDockWidget
{
	Q_OBJECT

public:
	struct Entry {
		Entry() : item(0), track(0), isTrack(false), occurrence(0) {}
		~Entry();

		PathItem *item;
		Track *track;
		bool isTrack;
		QString file;
		QString name;
		int occurrence;
	};

	FileListDock(QWidget *parent = 0);
	~FileListDock();

	void addData(const Data &data, const QString &fileName,
	  const QList<PathItem*> &paths);
	void clear();

	void setShowTracks(bool show);
	void setShowRoutes(bool show);
	void refreshColors();

	/* Visibility state that survives a reload of the files. */
	QHash<QString, bool> visibilityState() const;
	void restoreVisibilityState(const QHash<QString, bool> &state);
	QString selectedKey() const;
	void selectKey(const QString &key);

signals:
	void pathActivated(PathItem *item);
	void propertiesRequested(const FileListDock::Entry *entry);

private slots:
	void itemChanged(QTreeWidgetItem *item, int column);
	void currentItemChanged(QTreeWidgetItem *current,
	  QTreeWidgetItem *previous);
	void itemDoubleClicked(QTreeWidgetItem *item, int column);
	void filterChanged(const QString &text);
	void showContextMenu(const QPoint &pos);
	void showAll();
	void hideAll();
	void showSelectedProperties();

private:
	Entry *entry(QTreeWidgetItem *item) const;
	QString key(const Entry *e) const;
	void updateItem(QTreeWidgetItem *item);
	void applyVisibility(QTreeWidgetItem *item);
	void setAllChecked(bool checked);

	QTreeWidget *_tree;
	QLineEdit *_filter;
	QPushButton *_showAllButton, *_hideAllButton;
	QList<Entry*> _entries;
	PathItem *_selected;
	bool _showTracks, _showRoutes;
	bool _updating;
};

#endif // FILELISTDOCK_H
