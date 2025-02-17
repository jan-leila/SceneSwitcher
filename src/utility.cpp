#include "headers/utility.hpp"
#include "headers/platform-funcs.hpp"

#include <QTextStream>
#include <QLabel>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QtGui/qstandarditemmodel.h>
#include <QPropertyAnimation>
#include <QGraphicsColorizeEffect>
#include <QTimer>
#include <QMessageBox>
#include <unordered_map>
#include <regex>
#include <set>
#include <obs-module.h>
#include <util/util.hpp>

bool WeakSourceValid(obs_weak_source_t *ws)
{
	obs_source_t *source = obs_weak_source_get_source(ws);
	if (source) {
		obs_source_release(source);
	}
	return !!source;
}

std::string GetWeakSourceName(obs_weak_source_t *weak_source)
{
	std::string name;

	obs_source_t *source = obs_weak_source_get_source(weak_source);
	if (source) {
		name = obs_source_get_name(source);
		obs_source_release(source);
	}

	return name;
}

OBSWeakSource GetWeakSourceByName(const char *name)
{
	OBSWeakSource weak;
	obs_source_t *source = obs_get_source_by_name(name);
	if (source) {
		weak = obs_source_get_weak_source(source);
		obs_weak_source_release(weak);
		obs_source_release(source);
	}

	return weak;
}

OBSWeakSource GetWeakSourceByQString(const QString &name)
{
	return GetWeakSourceByName(name.toUtf8().constData());
}

OBSWeakSource GetWeakTransitionByName(const char *transitionName)
{
	OBSWeakSource weak;
	obs_source_t *source = nullptr;

	if (strcmp(transitionName, "Default") == 0) {
		source = obs_frontend_get_current_transition();
		weak = obs_source_get_weak_source(source);
		obs_source_release(source);
		obs_weak_source_release(weak);
		return weak;
	}

	obs_frontend_source_list *transitions = new obs_frontend_source_list();
	obs_frontend_get_transitions(transitions);
	bool match = false;

	for (size_t i = 0; i < transitions->sources.num; i++) {
		const char *name =
			obs_source_get_name(transitions->sources.array[i]);
		if (strcmp(transitionName, name) == 0) {
			match = true;
			source = transitions->sources.array[i];
			break;
		}
	}

	if (match) {
		weak = obs_source_get_weak_source(source);
		obs_weak_source_release(weak);
	}
	obs_frontend_source_list_free(transitions);

	return weak;
}

OBSWeakSource GetWeakTransitionByQString(const QString &name)
{
	return GetWeakTransitionByName(name.toUtf8().constData());
}

OBSWeakSource GetWeakFilterByName(OBSWeakSource source, const char *name)
{
	OBSWeakSource weak;
	auto s = obs_weak_source_get_source(source);
	if (s) {
		auto filterSource = obs_source_get_filter_by_name(s, name);
		weak = obs_source_get_weak_source(filterSource);
		obs_weak_source_release(weak);
		obs_source_release(filterSource);
		obs_source_release(s);
	}
	return weak;
}

OBSWeakSource GetWeakFilterByQString(OBSWeakSource source, const QString &name)
{
	return GetWeakFilterByName(source, name.toUtf8().constData());
}

std::string
getNextDelim(const std::string &text,
	     std::unordered_map<std::string, QWidget *> placeholders)
{
	size_t pos = std::string::npos;
	std::string res = "";

	for (const auto &ph : placeholders) {
		size_t newPos = text.find(ph.first);
		if (newPos <= pos) {
			pos = newPos;
			res = ph.first;
		}
	}

	if (pos == std::string::npos) {
		return "";
	}

	return res;
}

void placeWidgets(std::string text, QBoxLayout *layout,
		  std::unordered_map<std::string, QWidget *> placeholders,
		  bool addStretch)
{
	std::vector<std::pair<std::string, QWidget *>> labelsWidgetsPairs;

	std::string delim = getNextDelim(text, placeholders);
	while (delim != "") {
		size_t pos = text.find(delim);
		if (pos != std::string::npos) {
			labelsWidgetsPairs.emplace_back(text.substr(0, pos),
							placeholders[delim]);
			text.erase(0, pos + delim.length());
		}
		delim = getNextDelim(text, placeholders);
	}

	if (text != "") {
		labelsWidgetsPairs.emplace_back(text, nullptr);
	}

	for (auto &lw : labelsWidgetsPairs) {
		if (lw.first != "") {
			layout->addWidget(new QLabel(lw.first.c_str()));
		}
		if (lw.second) {
			layout->addWidget(lw.second);
		}
	}
	if (addStretch) {
		layout->addStretch();
	}
}

void clearLayout(QLayout *layout)
{
	QLayoutItem *item;
	while ((item = layout->takeAt(0))) {
		if (item->layout()) {
			clearLayout(item->layout());
			delete item->layout();
		}
		if (item->widget()) {
			delete item->widget();
		}
		delete item;
	}
}

bool compareIgnoringLineEnding(QString &s1, QString &s2)
{
	// Let QT deal with different types of lineendings
	QTextStream s1stream(&s1);
	QTextStream s2stream(&s2);

	while (!s1stream.atEnd() || !s2stream.atEnd()) {
		QString s1s = s1stream.readLine();
		QString s2s = s2stream.readLine();
		if (s1s != s2s) {
			return false;
		}
	}

	if (!s1stream.atEnd() && !s2stream.atEnd()) {
		return false;
	}

	return true;
}

std::string getSourceSettings(OBSWeakSource ws)
{
	auto s = obs_weak_source_get_source(ws);
	obs_data_t *data = obs_source_get_settings(s);
	std::string settings = obs_data_get_json(data);
	obs_data_release(data);
	obs_source_release(s);

	return settings;
}

void setSourceSettings(obs_source_t *s, const std::string &settings)
{
	if (settings.empty()) {
		return;
	}

	obs_data_t *data = obs_data_create_from_json(settings.c_str());
	if (!data) {
		blog(LOG_WARNING, "invalid source settings provided: \n%s",
		     settings.c_str());
		return;
	}
	obs_source_update(s, data);
	obs_data_release(data);
}

bool compareSourceSettings(const OBSWeakSource &source,
			   const std::string &settings, bool useRegex)
{
	bool ret = false;
	std::string currentSettings = getSourceSettings(source);
	if (useRegex) {
		try {
			std::regex expr(settings);
			ret = std::regex_match(currentSettings, expr);
		} catch (const std::regex_error &) {
		}
	} else {
		ret = currentSettings == settings;
	}
	return ret;
}

std::string getDataFilePath(const std::string &file)
{
	std::string root_path = obs_get_module_data_path(obs_current_module());
	if (!root_path.empty()) {
		return root_path + "/" + file;
	}
	return "";
}

bool DisplayMessage(const QString &msg, bool question)
{
	if (question) {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(
			nullptr, "Advanced Scene Switcher", msg,
			QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) {
			return true;
		} else {
			return false;
		}
	} else {
		QMessageBox Msgbox;
		Msgbox.setWindowTitle("Advanced Scene Switcher");
		Msgbox.setText(msg);
		Msgbox.exec();
	}

	return false;
}

void addSelectionEntry(QComboBox *sel, const char *description, bool selectable,
		       const char *tooltip)
{
	sel->insertItem(0, description);

	if (strcmp(tooltip, "") != 0) {
		sel->setItemData(0, tooltip, Qt::ToolTipRole);
	}

	QStandardItemModel *model =
		qobject_cast<QStandardItemModel *>(sel->model());
	QModelIndex firstIndex =
		model->index(0, sel->modelColumn(), sel->rootModelIndex());
	QStandardItem *firstItem = model->itemFromIndex(firstIndex);
	if (!selectable) {
		firstItem->setSelectable(false);
		firstItem->setEnabled(false);
	}
}

void populateSourceSelection(QComboBox *list, bool addSelect)
{
	auto enumSourcesWithSources = [](void *param, obs_source_t *source) {
		if (!source) {
			return true;
		}
		QComboBox *list = reinterpret_cast<QComboBox *>(param);
		list->addItem(obs_source_get_name(source));
		return true;
	};

	obs_enum_sources(enumSourcesWithSources, list);

	list->model()->sort(0);
	if (addSelect) {
		addSelectionEntry(
			list, obs_module_text("AdvSceneSwitcher.selectSource"),
			false);
	}
	list->setCurrentIndex(0);
}

void populateTransitionSelection(QComboBox *sel, bool addCurrent,
				 bool addSelect, bool selectable)
{

	obs_frontend_source_list *transitions = new obs_frontend_source_list();
	obs_frontend_get_transitions(transitions);

	for (size_t i = 0; i < transitions->sources.num; i++) {
		const char *name =
			obs_source_get_name(transitions->sources.array[i]);
		sel->addItem(name);
	}

	obs_frontend_source_list_free(transitions);

	sel->model()->sort(0);

	if (addCurrent) {
		sel->insertItem(
			0,
			obs_module_text("AdvSceneSwitcher.currentTransition"));
	}

	if (addSelect) {
		addSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectTransition"),
			selectable);
	}
	sel->setCurrentIndex(0);
}

void populateWindowSelection(QComboBox *sel, bool addSelect)
{

	std::vector<std::string> windows;
	GetWindowList(windows);

	for (std::string &window : windows) {
		sel->addItem(window.c_str());
	}

	sel->model()->sort(0);
	if (addSelect) {
		addSelectionEntry(
			sel, obs_module_text("AdvSceneSwitcher.selectWindow"));
	}
	sel->setCurrentIndex(0);
#ifdef WIN32
	sel->setItemData(0, obs_module_text("AdvSceneSwitcher.selectWindowTip"),
			 Qt::ToolTipRole);
#endif
}

void populateAudioSelection(QComboBox *sel, bool addSelect)
{

	auto sourceEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{
		std::vector<std::string> *list =
			reinterpret_cast<std::vector<std::string> *>(data);
		uint32_t flags = obs_source_get_output_flags(source);

		if ((flags & OBS_SOURCE_AUDIO) != 0) {
			list->push_back(obs_source_get_name(source));
		}
		return true;
	};

	std::vector<std::string> audioSources;
	obs_enum_sources(sourceEnum, &audioSources);

	for (std::string &source : audioSources) {
		sel->addItem(source.c_str());
	}

	sel->model()->sort(0);
	if (addSelect) {
		addSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectAudioSource"),
			false,
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
	}
	sel->setCurrentIndex(0);
}

void populateVideoSelection(QComboBox *sel, bool addSelect)
{

	auto sourceEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{
		std::vector<std::string> *list =
			reinterpret_cast<std::vector<std::string> *>(data);
		uint32_t flags = obs_source_get_output_flags(source);
		std::string test = obs_source_get_name(source);
		if ((flags & (OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC)) != 0) {
			list->push_back(obs_source_get_name(source));
		}
		return true;
	};

	std::vector<std::string> videoSources;
	obs_enum_sources(sourceEnum, &videoSources);
	sort(videoSources.begin(), videoSources.end());
	for (std::string &source : videoSources) {
		sel->addItem(source.c_str());
	}

	sel->model()->sort(0);
	if (addSelect) {
		addSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectVideoSource"),
			false,
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
	}
	sel->setCurrentIndex(0);
}

void populateMediaSelection(QComboBox *sel, bool addSelect)
{
	auto sourceEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{
		std::vector<std::string> *list =
			reinterpret_cast<std::vector<std::string> *>(data);
		std::string sourceId = obs_source_get_id(source);
		if (sourceId.compare("ffmpeg_source") == 0 ||
		    sourceId.compare("vlc_source") == 0) {
			list->push_back(obs_source_get_name(source));
		}
		return true;
	};

	std::vector<std::string> mediaSources;
	obs_enum_sources(sourceEnum, &mediaSources);
	for (std::string &source : mediaSources) {
		sel->addItem(source.c_str());
	}

	sel->model()->sort(0);
	if (addSelect) {
		addSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectMediaSource"),
			false,
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
	}
	sel->setCurrentIndex(0);
}

void populateProcessSelection(QComboBox *sel, bool addSelect)
{
	QStringList processes;
	GetProcessList(processes);
	processes.sort();
	for (QString &process : processes) {
		sel->addItem(process);
	}

	sel->model()->sort(0);
	if (addSelect) {
		addSelectionEntry(
			sel, obs_module_text("AdvSceneSwitcher.selectProcess"));
	}
	sel->setCurrentIndex(0);
}

void populateSceneSelection(QComboBox *sel, bool addPrevious,
			    bool addSceneGroup,
			    std::deque<SceneGroup> *sceneGroups, bool addSelect,
			    std::string selectText, bool selectable)
{
	BPtr<char *> scenes = obs_frontend_get_scene_names();
	char **temp = scenes;
	while (*temp) {
		const char *name = *temp;
		sel->addItem(name);
		temp++;
	}

	if (addPrevious) {
		sel->addItem(obs_module_text(
			"AdvSceneSwitcher.selectPreviousScene"));
	}

	if (addSceneGroup && sceneGroups) {
		for (auto &sg : *sceneGroups) {
			sel->addItem(QString::fromStdString(sg.name));
		}
	}

	sel->model()->sort(0);
	if (addSelect) {
		if (selectText.empty()) {
			addSelectionEntry(
				sel,
				obs_module_text("AdvSceneSwitcher.selectScene"),
				selectable,
				obs_module_text(
					"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
		} else {
			addSelectionEntry(sel, selectText.c_str(), selectable);
		}
	}
	sel->setCurrentIndex(0);
}

static inline void hasFilterEnum(obs_source_t *, obs_source_t *filter,
				 void *ptr)
{
	if (!filter) {
		return;
	}
	bool *hasFilter = reinterpret_cast<bool *>(ptr);
	*hasFilter = true;
}

void populateSourcesWithFilterSelection(QComboBox *list)
{
	auto enumSourcesWithFilters = [](void *param, obs_source_t *source) {
		if (!source) {
			return true;
		}
		QComboBox *list = reinterpret_cast<QComboBox *>(param);
		bool hasFilter = false;
		obs_source_enum_filters(source, hasFilterEnum, &hasFilter);
		if (hasFilter) {
			list->addItem(obs_source_get_name(source));
		}
		return true;
	};
	obs_enum_sources(enumSourcesWithFilters, list);
	obs_enum_scenes(enumSourcesWithFilters, list);
	list->model()->sort(0);
	addSelectionEntry(list,
			  obs_module_text("AdvSceneSwitcher.selectSource"));
	list->setCurrentIndex(0);
}

void populateFilterSelection(QComboBox *list, OBSWeakSource weakSource)
{
	auto enumFilters = [](obs_source_t *, obs_source_t *filter, void *ptr) {
		QComboBox *list = reinterpret_cast<QComboBox *>(ptr);
		auto name = obs_source_get_name(filter);
		list->addItem(name);
	};

	auto s = obs_weak_source_get_source(weakSource);
	obs_source_enum_filters(s, enumFilters, list);
	list->model()->sort(0);
	addSelectionEntry(list,
			  obs_module_text("AdvSceneSwitcher.selectFilter"));
	obs_source_release(s);
	list->setCurrentIndex(0);
}

static bool enumSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	std::set<QString> *names = reinterpret_cast<std::set<QString> *>(ptr);

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, enumSceneItem, ptr);
	}
	auto name = obs_source_get_name(obs_sceneitem_get_source(item));
	names->emplace(name);
	return true;
}

void populateSceneItemSelection(QComboBox *list, OBSWeakSource sceneWeakSource)
{
	std::set<QString> names;
	auto s = obs_weak_source_get_source(sceneWeakSource);
	auto scene = obs_scene_from_source(s);
	obs_scene_enum_items(scene, enumSceneItem, &names);
	obs_source_release(s);

	for (auto &name : names) {
		list->addItem(name);
	}
	list->model()->sort(0);
	addSelectionEntry(list, obs_module_text("AdvSceneSwitcher.selectItem"));
	list->setCurrentIndex(0);
}

QMetaObject::Connection PulseWidget(QWidget *widget, QColor endColor,
				    QColor startColor, QString specifier)
{
	widget->setStyleSheet(specifier + "{ \
		border-style: outset; \
		border-width: 2px; \
		border-radius: 10px; \
		border-color: rgb(0,0,0,0) \
		}");

	QGraphicsColorizeEffect *eEffect = new QGraphicsColorizeEffect(widget);
	widget->setGraphicsEffect(eEffect);
	QPropertyAnimation *paAnimation =
		new QPropertyAnimation(eEffect, "color");
	paAnimation->setStartValue(startColor);
	paAnimation->setEndValue(endColor);
	paAnimation->setDuration(1000);
	// Play backwards to return to original state on timer end
	paAnimation->setDirection(QAbstractAnimation::Backward);

	auto con = QWidget::connect(
		paAnimation, &QPropertyAnimation::finished, [paAnimation]() {
			QTimer::singleShot(1000, [paAnimation] {
				paAnimation->start();
			});
		});

	paAnimation->start();

	return con;
}

void listAddClicked(QListWidget *list, QWidget *newWidget,
		    QPushButton *addButton,
		    QMetaObject::Connection *addHighlight)
{
	if (!list || !newWidget) {
		blog(LOG_WARNING,
		     "listAddClicked called without valid list or widget");
		return;
	}

	if (addButton && addHighlight) {
		addButton->disconnect(*addHighlight);
	}

	QListWidgetItem *item;
	item = new QListWidgetItem(list);
	list->addItem(item);
	item->setSizeHint(newWidget->minimumSizeHint());
	list->setItemWidget(item, newWidget);

	list->scrollToItem(item);
}

bool listMoveUp(QListWidget *list)
{
	int index = list->currentRow();
	if (index == -1 || index == 0) {
		return false;
	}

	QWidget *row = list->itemWidget(list->currentItem());
	QListWidgetItem *itemN = list->currentItem()->clone();

	list->insertItem(index - 1, itemN);
	list->setItemWidget(itemN, row);

	list->takeItem(index + 1);
	list->setCurrentRow(index - 1);
	return true;
}

bool listMoveDown(QListWidget *list)
{
	int index = list->currentRow();
	if (index == -1 || index == list->count() - 1) {
		return false;
	}

	QWidget *row = list->itemWidget(list->currentItem());
	QListWidgetItem *itemN = list->currentItem()->clone();

	list->insertItem(index + 2, itemN);
	list->setItemWidget(itemN, row);

	list->takeItem(index);
	list->setCurrentRow(index + 1);
	return true;
}
