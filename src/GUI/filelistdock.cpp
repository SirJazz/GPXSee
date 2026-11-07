#include <QTreeWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include "data/data.h"
#include "data/track.h"
#include "pathitem.h"
#include "filelistdock.h"

#define ENTRY_ROLE (Qt::UserRole + 1)

class FileListItem : public QTreeWidgetItem
{
public:
	bool operator<(const QTreeWidgetItem &other) const
	{
		int column = treeWidget() ? treeWidget()->sortColumn() : 0;
		if (column == 1)
			return data(1, Qt::UserRole).toDouble()
			  < other.data(1, Qt::UserRole).toDouble();
		return text(0).localeAwareCompare(other.text(0)) < 0;
	}
};

FileListDock::Entry::~Entry()
{
	delete track;
}

static QIcon colorIcon(const QColor &color)
{
	QPixmap pm(16, 16);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing);
	p.setPen(QPen(QColor(0, 0, 0, 120), 1));
	p.setBrush(color.isValid() ? color : QColor(Qt::gray));
	p.drawRoundedRect(QRectF(1.5, 4.5, 13, 7), 2, 2);

	return QIcon(pm);
}

FileListDock::FileListDock(QWidget *parent)
  : QDockWidget(tr("Files"), parent), _selected(0), _showTracks(true),
  _showRoutes(true), _updating(false)
{
	setObjectName("FileListDock");
	setFeatures(QDockWidget::DockWidgetMovable
	  | QDockWidget::DockWidgetClosable);

	QWidget *widget = new QWidget(this);

	_tree = new QTreeWidget(widget);
	_tree->setColumnCount(2);
	_tree->setHeaderLabels(QStringList() << tr("Name") << tr("Distance"));
	_tree->setRootIsDecorated(false);
	_tree->setUniformRowHeights(true);
	_tree->setSortingEnabled(true);
	_tree->sortByColumn(0, Qt::AscendingOrder);
	_tree->setSelectionMode(QAbstractItemView::SingleSelection);
	_tree->setContextMenuPolicy(Qt::CustomContextMenu);
	_tree->header()->setStretchLastSection(false);
	_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

	_filter = new QLineEdit(widget);
	_filter->setPlaceholderText(tr("Filter"));
	_filter->setClearButtonEnabled(true);

	_showAllButton = new QPushButton(tr("Show all"), widget);
	_hideAllButton = new QPushButton(tr("Hide all"), widget);

	QHBoxLayout *buttons = new QHBoxLayout();
	buttons->setContentsMargins(0, 0, 0, 0);
	buttons->addWidget(_showAllButton);
	buttons->addWidget(_hideAllButton);

	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->setContentsMargins(2, 2, 2, 2);
	layout->addLayout(buttons);
	layout->addWidget(_tree);
	layout->addWidget(_filter);
	setWidget(widget);

	connect(_tree, &QTreeWidget::itemChanged, this,
	  &FileListDock::itemChanged);
	connect(_tree, &QTreeWidget::currentItemChanged, this,
	  &FileListDock::currentItemChanged);
	connect(_tree, &QTreeWidget::itemDoubleClicked, this,
	  &FileListDock::itemDoubleClicked);
	connect(_tree, &QWidget::customContextMenuRequested, this,
	  &FileListDock::showContextMenu);
	connect(_filter, &QLineEdit::textChanged, this,
	  &FileListDock::filterChanged);
	connect(_showAllButton, &QPushButton::clicked, this,
	  &FileListDock::showAll);
	connect(_hideAllButton, &QPushButton::clicked, this,
	  &FileListDock::hideAll);
}

FileListDock::~FileListDock()
{
	qDeleteAll(_entries);
}

FileListDock::Entry *FileListDock::entry(QTreeWidgetItem *item) const
{
	if (!item)
		return 0;
	int index = item->data(0, ENTRY_ROLE).toInt();
	return (index >= 0 && index < _entries.size()) ? _entries.at(index) : 0;
}

QString FileListDock::key(const Entry *e) const
{
	return QString("%1|%2|%3|%4").arg(e->file, e->isTrack ? "t" : "r",
	  e->name, QString::number(e->occurrence));
}

void FileListDock::addData(const Data &data, const QString &fileName,
  const QList<PathItem*> &paths)
{
	int tracks = data.tracks().count();
	QHash<QString, int> occurrences;
	QString canonical(QFileInfo(fileName).canonicalFilePath());
	QString file(canonical.isEmpty() ? fileName : canonical);

	_updating = true;
	_tree->setSortingEnabled(false);

	for (int i = 0; i < paths.size(); i++) {
		PathItem *pi = paths.at(i);
		bool isTrack = (i < tracks);
		QString name(isTrack ? data.tracks().at(i).name()
		  : data.routes().at(i - tracks).name());
		int occurrence = occurrences.value((isTrack ? "t" : "r") + name, 0);
		occurrences.insert((isTrack ? "t" : "r") + name, occurrence + 1);

		if (!pi)
			continue;

		Entry *e = new Entry();
		e->item = pi;
		e->isTrack = isTrack;
		e->file = file;
		e->name = name;
		e->occurrence = occurrence;
		if (isTrack)
			e->track = new Track(data.tracks().at(i));
		_entries.append(e);

		QTreeWidgetItem *item = new FileListItem();
		item->setData(0, ENTRY_ROLE, _entries.size() - 1);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(0, Qt::Checked);
		item->setText(0, name.isEmpty() ? QFileInfo(fileName).completeBaseName()
		  : name);
		item->setToolTip(0, QString("%1\n%2").arg(item->text(0),
		  QFileInfo(fileName).fileName()));
		qreal dist = pi->path().isEmpty() || pi->path().last().isEmpty()
		  ? 0 : pi->path().last().last().distance();
		item->setText(1, QString::number(dist / 1000.0, 'f', 1) + " km");
		item->setData(1, Qt::UserRole, dist);
		item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
		updateItem(item);
		_tree->addTopLevelItem(item);
		filterChanged(_filter->text());
	}

	_tree->setSortingEnabled(true);
	if (!_selected)
		_tree->setCurrentItem(0);
	_updating = false;
}

void FileListDock::clear()
{
	_updating = true;
	_tree->clear();
	qDeleteAll(_entries);
	_entries.clear();
	_selected = 0;
	_updating = false;
}

void FileListDock::updateItem(QTreeWidgetItem *item)
{
	Entry *e = entry(item);
	if (!e)
		return;

	item->setIcon(0, colorIcon(e->item->color()));
	QFont f(item->font(0));
	f.setItalic(!e->isTrack);
	item->setFont(0, f);
}

void FileListDock::refreshColors()
{
	for (int i = 0; i < _tree->topLevelItemCount(); i++)
		updateItem(_tree->topLevelItem(i));
}

void FileListDock::applyVisibility(QTreeWidgetItem *item)
{
	Entry *e = entry(item);
	if (!e)
		return;

	bool global = e->isTrack ? _showTracks : _showRoutes;
	e->item->setPathVisible(global && item->checkState(0) == Qt::Checked);
}

void FileListDock::setShowTracks(bool show)
{
	_showTracks = show;
	for (int i = 0; i < _tree->topLevelItemCount(); i++)
		applyVisibility(_tree->topLevelItem(i));
}

void FileListDock::setShowRoutes(bool show)
{
	_showRoutes = show;
	for (int i = 0; i < _tree->topLevelItemCount(); i++)
		applyVisibility(_tree->topLevelItem(i));
}

void FileListDock::itemChanged(QTreeWidgetItem *item, int column)
{
	if (_updating || column != 0)
		return;

	applyVisibility(item);
}

void FileListDock::currentItemChanged(QTreeWidgetItem *current,
  QTreeWidgetItem *previous)
{
	Q_UNUSED(previous);

	if (_selected) {
		_selected->hoverAll(false);
		_selected = 0;
	}
	if (_updating)
		return;

	Entry *e = entry(current);
	if (!e)
		return;

	if (current->checkState(0) != Qt::Checked) {
		current->setCheckState(0, Qt::Checked);
		applyVisibility(current);
	}
	_selected = e->item;
	_selected->hoverAll(true);
	emit pathActivated(_selected);
}

void FileListDock::itemDoubleClicked(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column);

	Entry *e = entry(item);
	if (e)
		emit propertiesRequested(e);
}

void FileListDock::showSelectedProperties()
{
	itemDoubleClicked(_tree->currentItem(), 0);
}

void FileListDock::filterChanged(const QString &text)
{
	for (int i = 0; i < _tree->topLevelItemCount(); i++) {
		QTreeWidgetItem *item = _tree->topLevelItem(i);
		item->setHidden(!text.isEmpty()
		  && !item->text(0).contains(text, Qt::CaseInsensitive));
	}
}

void FileListDock::setAllChecked(bool checked)
{
	_updating = true;
	for (int i = 0; i < _tree->topLevelItemCount(); i++) {
		QTreeWidgetItem *item = _tree->topLevelItem(i);
		if (item->isHidden())
			continue;
		item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
		applyVisibility(item);
	}
	_updating = false;
}

void FileListDock::showAll()
{
	setAllChecked(true);
}

void FileListDock::hideAll()
{
	setAllChecked(false);
}

void FileListDock::showContextMenu(const QPoint &pos)
{
	QTreeWidgetItem *item = _tree->itemAt(pos);
	QMenu menu(this);

	if (item) {
		_tree->setCurrentItem(item);
		menu.addAction(tr("Properties..."), this,
		  &FileListDock::showSelectedProperties);
		menu.addAction(tr("Show only this"), this, [this, item]() {
			setAllChecked(false);
			_updating = true;
			item->setCheckState(0, Qt::Checked);
			applyVisibility(item);
			_updating = false;
		});
		menu.addSeparator();
	}
	menu.addAction(tr("Show all"), this, &FileListDock::showAll);
	menu.addAction(tr("Hide all"), this, &FileListDock::hideAll);

	menu.exec(_tree->viewport()->mapToGlobal(pos));
}

QHash<QString, bool> FileListDock::visibilityState() const
{
	QHash<QString, bool> state;

	for (int i = 0; i < _tree->topLevelItemCount(); i++) {
		QTreeWidgetItem *item = _tree->topLevelItem(i);
		Entry *e = entry(item);
		if (e)
			state.insert(key(e), item->checkState(0) == Qt::Checked);
	}

	return state;
}

void FileListDock::restoreVisibilityState(const QHash<QString, bool> &state)
{
	_updating = true;
	for (int i = 0; i < _tree->topLevelItemCount(); i++) {
		QTreeWidgetItem *item = _tree->topLevelItem(i);
		Entry *e = entry(item);
		if (!e || !state.contains(key(e)))
			continue;
		item->setCheckState(0, state.value(key(e)) ? Qt::Checked
		  : Qt::Unchecked);
		applyVisibility(item);
	}
	_updating = false;
}

QString FileListDock::selectedKey() const
{
	Entry *e = entry(_tree->currentItem());
	return e ? key(e) : QString();
}

void FileListDock::selectKey(const QString &k)
{
	if (k.isEmpty())
		return;

	for (int i = 0; i < _tree->topLevelItemCount(); i++) {
		QTreeWidgetItem *item = _tree->topLevelItem(i);
		Entry *e = entry(item);
		if (e && key(e) == k) {
			_tree->setCurrentItem(item);
			_tree->scrollToItem(item);
			return;
		}
	}
}
